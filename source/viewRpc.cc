#include "viewRpc.h"
#include "viewLog.h"

namespace viewRpc {

    // *********************************************************************************************** //
    Channels::Channels(const std::string& service_name) : _service_name(service_name), _index(0) {}

    // 新增节点
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

    // 删除节点
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

    // 获取节点
    ChannelPtr Channels::Select() {
        std::unique_lock<std::mutex> lock(_mtx);
        if(_channel_vec.empty()) {
            return nullptr;
        }
        ChannelPtr channel = _channel_vec[_index];
        _index = (++_index) % _channel_vec.size();
        return channel;
    }

    // *********************************************************************************************** //

    Channels::Ptr RpcManager::GetService(const std::string& service_name) {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _services.find(service_name);
        if(it == _services.end()) {
            return nullptr;
        }
        return it->second;
    }

    // 关心服务: 是否对该服务进行管理
    void RpcManager::CareService(const std::string& service_name) {
        std::unique_lock<std::mutex> lock(_mtx);
        if(_services.find(service_name) != _services.end()) {
            return;
        }
        _services.insert(std::make_pair(service_name, std::make_shared<Channels>(service_name)));
    }
    // 新增节点
    void RpcManager::AddNode(const std::string& service_name, const std::string& addr) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::AddNode failed, service_name: {} not cared", service_name.c_str());
            return;
        }
        channels->Insert(addr);
    }

    // 删除节点
    void RpcManager::RemoveNode(const std::string& service_name, const std::string& addr) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::RemoveNode failed, service_name: {} not cared", service_name.c_str());
            return;
        }
        channels->Remove(addr);
    }

    // 获取节点
    ChannelPtr RpcManager::GetNode(const std::string& service_name) {
        Channels::Ptr channels = GetService(service_name);
        if(channels == nullptr) {
            viewLog::WARN("RpcManager::GetNode failed, service_name: {} not cared", service_name.c_str());
            return nullptr;
        }
        return channels->Select();
    }

    // *********************************************************************************************** //

    google::protobuf::Closure* ClosureFactory::Create(callback_t&& callback) {
        auto obj = std::make_shared<Object>();
        obj->callback = std::move(callback);
        return brpc::NewCallback(&ClosureFactory::AsyncCallBack, obj);
    }

    void ClosureFactory::AsyncCallBack(const Object::Ptr obj) {
        obj->callback();
    }

    // *********************************************************************************************** //
    // 作用: 创建 RPC 服务器
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