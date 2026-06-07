/**
 * @file viewEtcd.cc
 * @brief etcd 模块实现 —— 服务注册 / 服务发现
 */

#include "viewEtcd.h"
#include "viewUtil.h"
#include "viewLog.h"

#include <thread>
#include <chrono>
#include <future>

namespace viewEtcd {

// ==================== etcd 连接/重试配置常量 ====================
static const int MAX_CONNECT_RETRIES = 30;         ///< 最大连接重试次数
static const int MAX_RE_REGISTER_RETRIES = 5;      ///< 最大重新注册次数
static const int CONNECT_RETRY_INTERVAL_MS = 1000; ///< 连接重试间隔（毫秒）

// ==================== WaitForConnection 实现 ====================

/**
 * @brief 阻塞等待 etcd 连接建立成功
 * @param client etcd 客户端对象引用
 *
 * 每隔 1 秒尝试连接一次，最多重试 30 次。超时后输出错误日志并返回。
 */
void WaitForConnection(etcd::Client& client) {
    for (int retry = 0; retry < MAX_CONNECT_RETRIES; ++retry) {
        if (client.head().get().is_ok()) {
            return;
        }
        viewLog::INFO("等待 etcd 连接... ({}/{})", retry + 1, MAX_CONNECT_RETRIES);
        std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_RETRY_INTERVAL_MS));
    }
    viewLog::ERROR("etcd 连接超时，已重试 {} 次", MAX_CONNECT_RETRIES);
}

// ==================== ServiceRegister 实现 ====================

/**
 * @brief 构造服务注册器并生成唯一实例 ID
 * @param etcdHost       etcd 服务器地址
 * @param serviceName    服务名称
 * @param serviceAddress 服务访问地址（IP:Port 格式）
 */
ServiceRegister::ServiceRegister(const std::string& etcdHost, const std::string& serviceName, const std::string& serviceAddress) 
    : _etcdHost(etcdHost), _serviceName(serviceName), _serviceAddress(serviceAddress) {
    // 构造函数中可以进行一些初始化操作，例如生成实例 ID
    _instanceId = viewUtil::RandomUtil::RandomString(); // 生成随机的实例 ID
}

/**
 * @brief 生成 etcd 中存储的完整 key 路径
 * @return 格式为 "/serviceName/instanceId" 的字符串
 */
std::string ServiceRegister::MakeKey() {
    std::stringstream ss;
    ss << "/" << _serviceName << "/" << _instanceId;
    return ss.str();
}

/**
 * @brief 执行服务注册到 etcd
 *
 * 注册流程：
 * 1. 创建 etcd 客户端并等待连接就绪
 * 2. 创建 3 秒租约
 * 3. 写入 Key=/serviceName/instanceId, Value=serviceAddress
 * 4. 启动心跳保活，失败时自动重新注册（最多 5 次）
 *
 * @example
 * @code
 * // Key: /user/<instanceId> → Value: 192.168.10.129:9000
 * bool ret = register->RegisterService();
 * @endcode
 */
bool ServiceRegister::RegisterService() {
    // ===================== 1. 创建 etcd 客户端 =====================
    etcd::Client client(_etcdHost);

    // ===================== 2. 等待连接成功 =====================
    WaitForConnection(client);

    // ===================== 3. 创建 3 秒租约 =====================
    auto leaseResp = client.leasegrant(3).get();
    if (!leaseResp.is_ok()) {
        viewLog::ERROR("创建租约失败: {}", leaseResp.error_message());
        return false;
    }

    // 获取租约ID（必须有效）
    int64_t leaseId = leaseResp.value().lease();
    if (leaseId <= 0) {
        viewLog::ERROR("租约ID非法");
        return false;
    }

    // ===================== 4. 构造服务注册 key / value =====================
    std::string key   = this->MakeKey();
    std::string value = _serviceAddress;

    // ===================== 5. 写入注册信息（绑定租约） =====================
    auto putResp = client.put(key, value, leaseId).get();
    if (!putResp.is_ok()) {
        viewLog::ERROR("服务注册失败: {}", putResp.error_message());
        return false;
    }

    // ===================== 6. 【关键】先停止旧的保活，避免多心跳 =====================
    if (_keepAlive) {
        _keepAlive->Cancel();  // 停止旧心跳
        _keepAlive.reset();    // 释放对象
    }

    // ===================== 7. 保活回调：保活失败时进行有限的重新注册 =====================
    std::weak_ptr<ServiceRegister> weak_self = shared_from_this();
    // shared_ptr 共享重试计数，跨回调调用保持状态
    auto retryCount = std::make_shared<int>(0);

    auto handler = [weak_self, retryCount](const std::exception_ptr& ex) {
        auto self = weak_self.lock();
        if (!self) {
            viewLog::WARN("ServiceRegister 对象已销毁，停止保活重试");
            return;
        }

        try {
            if (ex) std::rethrow_exception(ex);
        } catch (const std::exception& e) {
            viewLog::ERROR("保活失败: {}", e.what());
        }

        // 限制重新注册次数，避免无限递归
        if (*retryCount >= MAX_RE_REGISTER_RETRIES) {
            viewLog::ERROR("重新注册已达最大重试次数 {}，停止重试", MAX_RE_REGISTER_RETRIES);
            return;
        }
        ++(*retryCount);

        viewLog::INFO("将在 1 秒后尝试重新注册 ({}/{})",
                      *retryCount, MAX_RE_REGISTER_RETRIES);

        // 延迟重试，移除 std::async 避免 future 析构阻塞
        std::this_thread::sleep_for(std::chrono::seconds(1));
        self->RegisterService();
    };

    // ===================== 8. 创建新保活 =====================
    _keepAlive = std::make_shared<etcd::KeepAlive>(_etcdHost, handler, 3, leaseId);

    viewLog::INFO("服务注册成功 key:{} value:{}", key, value);
    return true;
}

// ==================== ServiceDiscovery 实现 ====================

/**
 * @brief 构造服务发现器
 * @param etcdHost        etcd 服务器地址
 * @param onlineCallback  服务上线回调函数
 * @param offlineCallback 服务下线回调函数
 */
ServiceDiscovery::ServiceDiscovery(const std::string& etcdHost, ModifyCallback onlineCallback, ModifyCallback offlineCallback) 
    : _etcdHost(etcdHost), _onlineCallback(onlineCallback), _offlineCallback(offlineCallback) {}

/**
 * @brief etcd watch 事件处理函数
 * @param resp etcd 返回的响应数据
 *
 * 解析事件类型：
 * - "put" → 调用上线回调
 * - "delete" → 调用下线回调
 * - 其他 → 输出日志
 */
void ServiceDiscovery::WatchHandler(const etcd::Response& resp) {
    if(!resp.is_ok()) {
        viewLog::ERROR("Watch 失败: {}", resp.error_message());
    } else {
        // 处理事件，根据事件类型调用对应的回调函数
        std::string serviceName = ParseKey(resp.value().key());
        std::string serviceAddr = resp.value().as_string();
        
        if (resp.action() == "put") {
            viewLog::INFO("服务上线: key:{} value:{}", resp.value().key(), serviceAddr);
            if (_onlineCallback) {
                _onlineCallback(serviceName, serviceAddr);
            }
        } else if (resp.action() == "delete") {
            viewLog::INFO("服务下线: key:{} value:{}", resp.value().key(), serviceAddr);
            if (_offlineCallback) {
                _offlineCallback(serviceName, serviceAddr);
            }
        } else {
            viewLog::INFO("无效事件通知... action: {}, key: {}, value: {}", resp.action(), resp.value().key(), serviceAddr);
        }
    }
}

/**
 * @brief 从完整 key 路径中提取服务名称
 * @param key etcd 中的完整 key（格式：/serviceName/instanceId）
 * @return 提取出的 serviceName，解析失败返回空字符串
 */
std::string ServiceDiscovery::ParseKey(const std::string& key) {
    // 假设 key 格式为 /serviceName/instanceId
    std::vector<std::string> parts;
    viewUtil::StrUtil::Split(key, "/", &parts);
    return parts.size() >= 2 ? parts[1] : ""; // 返回 serviceName
}

/**
 * @brief 启动服务发现——递归监控 etcd 根目录 "/"
 *
 * 流程：
 * 1. 创建 etcd 客户端并等待连接
 * 2. 列出 "/" 下已有节点，触发上线回调
 * 3. 创建 Watcher 递归监控所有变化，通过 WatchHandler 分发事件
 */
bool ServiceDiscovery::WatchService() {
    // 实例化客户端对象
    etcd::Client client(_etcdHost);
    // 等待连接成功
    WaitForConnection(client);
    // 创建 Watcher 对象，监视 / 目录下的所有变化
    auto resp = client.ls("/").get();
    if(!resp.is_ok()) {
        viewLog::ERROR("获取目录列表失败: {}", resp.error_message());
        return false;
    }
    auto values = resp.values();
    for (const auto& v : values) {
        std::string serviceName = ParseKey(v.key());
        std::string serviceAddr = v.as_string();
        viewLog::INFO("已有节点: key:{} value:{}", v.key(), v.as_string());
        if(_onlineCallback) _onlineCallback(serviceName, serviceAddr);
    }
    // 监控根目录，获取服务上下线的通知进行对应处理
    auto cb = std::bind(&ServiceDiscovery::WatchHandler, this, std::placeholders::_1);
    _watcher.reset( new etcd::Watcher(_etcdHost, "/", cb, true)); // true 表示递归监控目录下所有数据
    _watcher->Wait([this](bool cond) {
        if (cond == true) { return; }
        this->WatchService();
    });
    return true;
}

} // namespace viewEtcd