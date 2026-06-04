/**
 * @file viewRpc.h
 * @brief RPC 模块 —— 基于 brpc 的 RPC 客户端/服务端封装
 * @author Your Name
 * @date 2026
 *
 * 该模块封装了 brpc 的 Channel、Server、Closure 等功能，提供：
 * - Channels：单服务多节点 Channel 连接池（轮询负载均衡）
 * - RpcManager：多服务 Channel 管理器
 * - ClosureFactory：支持 lambda 回调的 Closure 工厂
 * - ServerFactory：RPC 服务端快捷创建工厂
 */

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

/**
 * @class Channels
 * @brief 单服务 Channel 连接池
 *
 * 管理同一服务的多个 Channel 节点，提供轮询（Round-Robin）选择策略。
 * 线程安全，支持动态增删节点。
 */
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

/**
 * @class RpcManager
 * @brief 多服务 RPC 管理器
 *
 * 管理多个服务的 Channels 连接池，按服务名称索引。
 * 使用前需要先调用 CareService() 声明关心的服务，然后通过 AddNode/RemoveNode/GetNode 操作节点。
 */
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

/**
 * @class ClosureFactory
 * @brief RPC Closure 工厂类
 *
 * 将 std::function 回调包装为 brpc 的 google::protobuf::Closure 对象，
 * 支持 lambda 表达式和仿函数。
 */
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

/**
 * @class ServerFactory
 * @brief RPC 服务端工厂类
 *
 * 快速创建并启动 brpc 服务器，自动接管 Service 对象生命周期。
 * 创建的服务器空闲超时为无限（不主动断开空闲连接）。
 */
class ServerFactory {
public:
// 默认 service 是堆上 new 出来的, 将管理权交给 RPC 服务器进行管理
    static std::shared_ptr<brpc::Server> Create(int port, google::protobuf::Service* service);
};

} // namespace viewRpc