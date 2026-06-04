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

// etcd 连接重试配置
static const int MAX_CONNECT_RETRIES = 30;        // 最大连接重试次数
static const int MAX_RE_REGISTER_RETRIES = 5;     // 最大重新注册次数
static const int CONNECT_RETRY_INTERVAL_MS = 1000; // 连接重试间隔（毫秒）

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

// *********************************************************************************************** //


ServiceRegister::ServiceRegister(const std::string& etcdHost, const std::string& serviceName, const std::string& serviceAddress) 
    : _etcdHost(etcdHost), _serviceName(serviceName), _serviceAddress(serviceAddress) {
    // 构造函数中可以进行一些初始化操作，例如生成实例 ID
    _instanceId = viewUtil::RandomUtil::RandomString(); // 生成随机的实例 ID
}

std::string ServiceRegister::MakeKey() {
    std::stringstream ss;
    ss << "/" << _serviceName << "/" << _instanceId;
    return ss.str();
}

// bool ret = RegisterService("user", "192.168.10.129:9000");
// -> Key: /user/_instanceId -> Value: 192.168.10.129:9000
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

// *********************************************************************************************** //

ServiceDiscovery::ServiceDiscovery(const std::string& etcdHost, ModifyCallback onlineCallback, ModifyCallback offlineCallback) 
    : _etcdHost(etcdHost), _onlineCallback(onlineCallback), _offlineCallback(offlineCallback) {}

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
std::string ServiceDiscovery::ParseKey(const std::string& key) {
    // 假设 key 格式为 /serviceName/instanceId
    std::vector<std::string> parts;
    viewUtil::StrUtil::Split(key, "/", parts);
    return parts.size() >= 2 ? parts[1] : ""; // 返回 serviceName
}

// 监控 / 目录, 并不针对单独的数据
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