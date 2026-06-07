/**
 * @file viewEtcd.h
 * @brief etcd 服务注册与发现模块
 * @author Your Name
 * @date 2026
 *
 * 基于 etcd-cpp-api 的服务注册与发现功能封装，提供：
 * - WaitForConnection： 阻塞等待 etcd 连接就绪
 * - ServiceRegister：   服务注册，支持租约心跳保活与失败自动重注册
 * - ServiceDiscovery：  服务发现，监听目录变化并回调通知上线/下线事件
 */

#pragma once
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>

#include <string>

namespace viewEtcd {

/**
 * @brief 等待 etcd 连接建立成功
 * @param client etcd 客户端对象引用
 * @note 该函数会阻塞直到与 etcd 服务端成功建立连接，最多重试 30 次（每次间隔 1 秒）。
 *       超时后输出错误日志并返回，不会无限阻塞。
 */
void WaitForConnection(etcd::Client& client);

/**
 * @class ServiceRegister
 * @brief 服务注册类
 *
 * 用于将服务实例注册到 etcd 中，支持自动心跳保活和失败自动重新注册。
 * 每个服务节点对应一个独立的注册实例，通过唯一实例 ID 区分。
 *
 * 线程安全说明：本类非线程安全，单节点单服务的典型使用模式足够。
 * 如需支持动态重新注册，请外部加锁保护。
 *
 * 生命周期说明：使用 weak_ptr 防止 ServiceRegister 对象提前析构时
 * callback 仍在运行导致崩溃。继承自 enable_shared_from_this。
 */
class ServiceRegister : public std::enable_shared_from_this<ServiceRegister> {
public:
    using Ptr = std::shared_ptr<ServiceRegister>; ///< ServiceRegister 智能指针类型别名

    /**
     * @brief 构造服务注册器
     * @param etcdHost       etcd 服务器地址（例如：http://127.0.0.1:2379）
     * @param serviceName    服务名称（作为 etcd 中的 key 前缀）
     * @param serviceAddress 本服务实例的访问地址（IP:Port 格式）
     */
    ServiceRegister(const std::string& etcdHost,
                   const std::string& serviceName,
                   const std::string& serviceAddress);

    /**
     * @brief 执行服务注册
     * @return true 注册成功，false 注册失败
     *
     * 在 etcd 中创建 /serviceName/instanceId 节点，并启动心跳保活机制。
     * 保活失败时会自动重新注册，最多重试 5 次。
     *
     * @note 注册成功后会自动维持心跳，无需额外调用保活接口。
     */
    bool RegisterService();

protected:
    /**
     * @brief 生成 etcd 中存储的完整 key 路径
     * @return 格式为 "/serviceName/instanceId" 的字符串
     */
    std::string MakeKey();

private:
    std::string _etcdHost;                      ///< etcd服务器地址
    std::string _instanceId;                    ///< 实例唯一标识（通常为UUID或主机名+进程ID）
    std::string _serviceName;                   ///< 服务名称
    std::string _serviceAddress;                ///< 服务访问地址（格式：IP:Port）
    std::shared_ptr<etcd::KeepAlive> _keepAlive; ///< 心跳保活对象，自动续租服务注册信息
};

/**
 * @class ServiceDiscovery
 * @brief 服务发现类
 *
 * 监视 etcd 中指定服务目录的变化，实时感知服务上下线事件。
 * 启动后会先列出已有节点并触发上线回调，然后持续监听目录变化。
 */
class ServiceDiscovery {
public:
    using Ptr = std::shared_ptr<ServiceDiscovery>; ///< ServiceDiscovery 智能指针类型别名

    /**
     * @brief 服务变化回调函数类型
     * @param serviceName 服务名称
     * @param serviceAddr 发生变化的服务实例地址（IP:Port 格式）
     */
    using ModifyCallback = std::function<void(const std::string& serviceName,
                                              std::string& serviceAddr)>;

    /**
     * @brief 构造服务发现器
     * @param etcdHost        etcd 服务器地址
     * @param onlineCallback  服务上线时的回调函数（新增服务节点时触发）
     * @param offlineCallback 服务下线时的回调函数（服务节点消失时触发）
     */
    ServiceDiscovery(const std::string& etcdHost,
                    ModifyCallback onlineCallback,
                    ModifyCallback offlineCallback);

    /**
     * @brief 开始监视服务变化
     * @return true 监视启动成功，false 启动失败
     *
     * 监视根目录 "/"，捕获所有服务的变化事件，通过回调函数通知上层。
     * 先列出已有节点触发上线回调，再启动 Watcher 持续监听。
     */
    bool WatchService();

protected:
    /**
     * @brief etcd watch 事件处理函数
     * @param resp etcd 返回的响应数据，包含事件类型和键值信息
     *
     * 解析事件类型（PUT/DELETE），分别调用上线或下线回调。
     */
    void WatchHandler(const etcd::Response& resp);

    /**
     * @brief 从完整 key 路径中提取服务名称
     * @param key etcd 中的完整 key（格式：/serviceName/instanceId）
     * @return 提取出的 serviceName，解析失败返回空字符串
     */
    std::string ParseKey(const std::string& key);

private:
    std::string _etcdHost;                          ///< etcd服务器地址
    ModifyCallback _onlineCallback;                 ///< 服务上线回调函数
    ModifyCallback _offlineCallback;                ///< 服务下线回调函数
    std::shared_ptr<etcd::Watcher> _watcher;        ///< etcd监视器，持续监听服务变化
};

} // namespace viewEtcd