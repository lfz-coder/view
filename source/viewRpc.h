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
    using Ptr = std::shared_ptr<Channels>; ///< Channels 智能指针类型别名

    /**
     * @brief 构造函数
     * @param service_name 服务名称
     */
    Channels(const std::string& service_name);
    ~Channels() = default;

    /**
     * @brief 新增一个服务节点
     * @param addr 节点地址（格式：IP:Port），重复添加会被忽略
     */
    void Insert(const std::string& addr);

    /**
     * @brief 删除一个服务节点
     * @param addr 节点地址，不存在时输出警告日志
     */
    void Remove(const std::string& addr);

    /**
     * @brief 轮询选择一个服务节点
     * @return 选中的 Channel 智能指针，无可用节点时返回 nullptr
     */
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

    /**
     * @brief 声明关心的服务——为该服务创建 Channels 连接池
     * @param service_name 服务名称，重复声明会被忽略
     */
    void CareService(const std::string& service_name);

    /**
     * @brief 为指定服务新增节点
     * @param service_name 服务名称（需先调用 CareService）
     * @param addr         节点地址
     */
    void AddNode(const std::string& service_name, const std::string& addr);

    /**
     * @brief 为指定服务删除节点
     * @param service_name 服务名称
     * @param addr         节点地址
     */
    void RemoveNode(const std::string& service_name, const std::string& addr);

    /**
     * @brief 从指定服务中获取一个可用节点（轮询）
     * @param service_name 服务名称
     * @return 选中的 Channel，服务不存在或无可用节点时返回 nullptr
     */
    ChannelPtr GetNode(const std::string& service_name);

protected:
    /**
     * @brief 获取指定服务的 Channels 连接池
     * @param service_name 服务名称
     * @return Channels 智能指针，不存在时返回 nullptr
     */
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
    using callback_t = std::function<void()>; ///< 回调函数类型别名

    /**
     * @brief 创建 brpc Closure 对象——将 std::function 回调包装为 protobuf Closure
     * @param callback 用户回调函数（支持 lambda 表达式和仿函数）
     * @return brpc Closure 指针，由 brpc 框架负责释放
     */
    static google::protobuf::Closure* Create(callback_t&& callback);

private:
    /// 内部包装对象，持有用户回调
    struct Object {
        using Ptr = std::shared_ptr<Object>; ///< Object 智能指针类型别名
        callback_t callback;                 ///< 用户回调函数
    };

    /**
     * @brief 异步回调入口——brpc 回调时调用
     * @param obj 持有用户回调的 Object 智能指针
     */
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
    /**
     * @brief 创建并启动 brpc 服务器
     * @param port    监听端口
     * @param service protobuf Service 对象指针（堆上创建，生命周期由 brpc 接管）
     * @return 成功返回 Server 智能指针，失败返回 nullptr
     *
     * @note service 由 brpc 框架接管生命周期（SERVER_OWNS_SERVICE），调用者无需手动释放。
     * @note 服务器空闲超时设为 -1（不主动断开空闲连接）。
     */
    static std::shared_ptr<brpc::Server> Create(int port, google::protobuf::Service* service);
};

} // namespace viewRpc