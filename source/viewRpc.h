#pragma once
#include <butil/logging.h>
#include <brpc/channel.h>
#include <brpc/server.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
namespace viewRpc {
using ChannelPtr = std::shared_ptr<brpc::Channel>;
// 单服务集合
class Channels {
public:
    using Ptr = std::shared_ptr<Channels>;
    Channels(const std::string& service_name);
    ~Channels() = default;

    // 新增节点
    void Insert(const std::string& addr);
    // 删除节点
    void Remove(const std::string& addr);
    // 获取节点
    ChannelPtr Select();

private:
    std::mutex _mtx;
    std::string _service_name; // 服务名称
    uint32_t _index; // 轮询索引
    std::vector<ChannelPtr> _channel_vec;
    std::unordered_map<std::string, ChannelPtr> _channels; // 服务地址 -> Channel
};

// 服务管理类
class RpcManager {
public:
    RpcManager() = default;
    ~RpcManager() = default;

    // 关心服务: 是否对该服务进行管理
    void CareService(const std::string& service_name);
    // 新增节点
    void AddNode(const std::string& service_name, const std::string& addr);
    // 删除节点
    void RemoveNode(const std::string& service_name, const std::string& addr);
    // 获取节点
    ChannelPtr GetNode(const std::string& service_name);

protected:
    // 获取服务
    Channels::Ptr GetService(const std::string& service_name);

private:
    std::mutex _mtx;
    std::unordered_map<std::string, Channels::Ptr> _services;
};

class ClosureFactory {
public:
    using callback_t = std::function<void()>;
    static google::protobuf::Closure* Create(callback_t&& callback);
private:
    // 支持仿函数和 lambda 表达式
    struct Object {
        using Ptr = std::shared_ptr<Object>;
        callback_t callback;
    };

    static void AsyncCallBack(const Object::Ptr obj);

};

// Server 工厂类
class ServerFactory {
public:
// 默认 service 是堆上 new 出来的, 将管理权交给 RPC 服务器进行管理
    static std::shared_ptr<brpc::Server> Create(int port, google::protobuf::Service* service);
};

} // namespace view