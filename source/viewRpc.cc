/**
 * @file viewRpc.cc
 * @brief RPC 模块实现 —— Channels / RpcManager / ClosureFactory / ServerFactory
 */

#include "viewRpc.h"
#include "viewLog.h"

namespace viewRpc {

    // ==================== Channels 实现 ====================

    /**
     * @brief 构造函数——初始化服务名称和轮询索引
     * @param service_name 服务名称
     */
    Channels::Channels(const std::string& service_name) : _service_name(service_name), _index(0) {}

    /**
     * @brief 新增一个服务节点到连接池
     * @param addr 节点地址（IP:Port），重复添加会被忽略
     *
     * 创建 brpc::Channel 并初始化（baidu_std 协议，1 秒超时，3 次重试），
     * 同时维护 _channels 哈希表和 _channel_vec 向量以支持轮询选择。
     */
    void Channels::Insert(const std::string& addr) {
        std::unique_lock<std::mutex> lock(_mtx);
        if(_channels.find(addr) != _channels.end()) {
            return;
        }
        ChannelPtr channel = std::make_shared<brpc::Channel>();
        brpc::ChannelOptions options;
        options.protocol = "baidu_std"; // 使用百度标准协议
        options.timeout_ms = 1000; // 1 second.
        options.max_retry = 3; // 3 retries.
        if (channel->Init(addr.c_str(), &options) != 0) {
            // 初始化失败，记录日志并返回
            return;
        }
        _channels.insert(std::make_pair(addr, channel));
        _channel_vec.push_back(channel);
    }

    /**
     * @brief 从连接池中删除指定节点
     * @param addr 节点地址，不存在时输出警告日志
     */
    void Channels::Remove(const std::string& addr) {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _channels.find(addr);
        if(it == _channels.end()) {
            viewLog::WARN("Channels::Remove failed, addr: {} not found", addr.c_str());
            return;
        }
        auto willDelChannel = it->second;
        _channels.erase(it);
        _channel_vec.erase(std::remove(_channel_vec.begin(), _channel_vec.end(), willDelChannel), _channel_vec.end());
    }

    /**
     * @brief 轮询选择一个可用节点（Round-Robin 策略）
     * @return 选中的 Channel 智能指针，无可用节点时返回 nullptr
     */
    ChannelPtr Channels::Select() {
        std::unique_lock<std::mutex> lock(_mtx);
        if(_channel_vec.empty()) {
            return nullptr;
        }
        ChannelPtr channel = _channel_vec[_index];
        _index = (_index + 1) % _channel_vec.size();
        return channel;
    }

    // ==================== RpcManager 实现 ====================

    /**
     * @brief 获取指定服务的 Channels 连接池（内部方法）
     * @param service_name 服务名称
     * @return Channels 智能指针，不存在时返回 nullptr
     */
    Channels::Ptr RpcManager::GetService(const std::string& service_name) {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _services.find(service_name);
        if(it == _services.end()) {
            return nullptr;
        }
        return it->second;
    }

    /**
     * @brief 声明关心的服务——为该服务创建 Channels 连接池
     * @param service_name 服务名称，重复声明会被忽略
     */
    void RpcManager::CareService(const std::string& service_name) {
        std::unique_lock<std::mutex> lock(_mtx);
        if(_services.find(service_name) != _services.end()) {
            return;
        }
        _services.insert(std::make_pair(service_name, std::make_shared<Channels>(service_name)));
    }
    /**
     * @brief 为指定服务新增节点
     * @param service_name 服务名称（需先调用 CareService）
     * @param addr         节点地址
     */
    void RpcManager::AddNode(const std::string& service_name, const std::string& addr) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::AddNode failed, service_name: {} not cared", service_name.c_str());
            return;
        }
        channels->Insert(addr);
    }

    /**
     * @brief 为指定服务删除节点
     * @param service_name 服务名称
     * @param addr         节点地址
     */
    void RpcManager::RemoveNode(const std::string& service_name, const std::string& addr) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::RemoveNode failed, service_name: {} not cared", service_name.c_str());
            return;
        }
        channels->Remove(addr);
    }

    /**
     * @brief 从指定服务中获取一个可用节点（轮询）
     * @param service_name 服务名称
     * @return 选中的 Channel，服务不存在或无可用节点时返回 nullptr
     */
    ChannelPtr RpcManager::GetNode(const std::string& service_name) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::GetNode failed, service_name: {} not cared", service_name.c_str());
            return nullptr;
        }
        return channels->Select();
    }

    // ==================== ClosureFactory 实现 ====================

    /**
     * @brief 创建 brpc Closure 对象——将 std::function/lambda 包装为 protobuf Closure
     * @param callback 用户回调函数（支持 lambda 和仿函数）
     * @return brpc Closure 指针，由 brpc 框架负责释放
     */
    google::protobuf::Closure* ClosureFactory::Create(callback_t&& callback) {
        auto obj = std::make_shared<Object>();
        obj->callback = std::move(callback);
        return brpc::NewCallback(&ClosureFactory::AsyncCallBack, obj);
    }

    /**
     * @brief brpc 异步回调入口——转发到用户 callback
     * @param obj 持有用户回调的 Object 智能指针
     */
    void ClosureFactory::AsyncCallBack(const Object::Ptr obj) {
        obj->callback();
    }

    // ==================== ServerFactory 实现 ====================

    /**
     * @brief 创建并启动 brpc 服务器
     * @param port    监听端口
     * @param service protobuf Service 对象指针（生命周期由 brpc 接管，SERVER_OWNS_SERVICE）
     * @return 成功返回 Server 智能指针，失败返回 nullptr
     *
     * @note 服务器空闲超时设为 -1（不主动断开空闲连接）。
     */
    std::shared_ptr<brpc::Server> ServerFactory::Create(int port, google::protobuf::Service* service) {
        auto server = std::make_shared<brpc::Server>();
        if (server->AddService(service, brpc::SERVER_OWNS_SERVICE) != 0) {
            viewLog::ERROR("ServerFactory::Create failed, add service to server failed");
            return nullptr;
        }
        brpc::ServerOptions options;
        options.idle_timeout_sec = -1; // 连接最大空闲时间，单位秒。设置为-1表示不关闭空闲连接
        if (server->Start(port, &options) != 0) {
            viewLog::ERROR("ServerFactory::Create failed, start server on port: {} failed", port);
            return nullptr;
        }
        return server;
    }

}