#pragma once
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>

#include <string>

namespace viewEtcd {

// 注意：下面的类非线程安全，但单节点单服务的典型使用模式足够
// 如需支持动态重新注册，请外部加锁保护

/**
 * @brief 等待etcd连接建立成功
 * @param client etcd客户端对象引用
 * @note 该函数会阻塞直到与etcd服务端成功建立连接[每隔固定时间会进行重连]
 */
void WaitForConnection(etcd::Client& client);

/**
 * @brief 服务注册类
 * @details 用于将服务实例注册到etcd中，支持自动心跳保活。
 *          每个服务节点对应一个独立的注册实例，通过唯一实例ID区分。
 *          如果 ServiceRegister 对象被提前析构，callback 还在运行就会崩溃
 *          解决方案：使用 weak_ptr
 */
class ServiceRegister : public std::enable_shared_from_this<ServiceRegister> {
public:
    using Ptr = std::shared_ptr<ServiceRegister>;

    /**
     * @brief 构造服务注册器
     * @param etcdHost etcd服务器地址（例如：http://127.0.0.1:2379）
     * @param serviceName 服务名称（作为etcd中的key前缀）
     * @param serviceAddress 本服务实例的访问地址（IP:Port格式）
     */
    ServiceRegister(const std::string& etcdHost, 
                   const std::string& serviceName, 
                   const std::string& serviceAddress);

    /**
     * @brief 执行服务注册
     * @return true 注册成功；false 注册失败
     * @details 在etcd中创建 /serviceName/instanceId 节点，并启动心跳保活机制
     * @note 注册成功后会自动维持心跳，无需额外调用保活接口
     */
    bool RegisterService();

protected:
    /**
     * @brief 生成etcd中存储的完整key路径
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
 * @brief 服务发现类
 * @details 监视etcd中指定服务目录的变化，实时感知服务上下线事件
 */
class ServiceDiscovery {
public:
    using Ptr = std::shared_ptr<ServiceDiscovery>;

    /**
     * @brief 服务变化回调函数类型
     * @param serviceName 服务名称
     * @param serviceAddr 发生变化的服务实例地址（IP:Port格式）
     */
    using ModifyCallback = std::function<void(const std::string& serviceName, 
                                              std::string& serviceAddr)>;

    /**
     * @brief 构造服务发现器
     * @param etcdHost etcd服务器地址
     * @param onlineCallback 服务上线时的回调函数（新增服务节点时触发）
     * @param offlineCallback 服务下线时的回调函数（服务节点消失时触发）
     */
    ServiceDiscovery(const std::string& etcdHost, 
                    ModifyCallback onlineCallback, 
                    ModifyCallback offlineCallback);

    /**
     * @brief 开始监视服务变化
     * @return true 监视启动成功；false 启动失败
     * @details 监视根目录"/"，捕获所有服务的变化事件，通过回调函数通知上层
     */
    bool WatchService();

protected:
    /**
     * @brief etcd watch事件处理函数
     * @param resp etcd返回的响应数据，包含事件类型和键值信息
     * @details 解析事件类型（PUT/DELETE），分别调用上线或下线回调
     */
    void WatchHandler(const etcd::Response& resp);

    /**
     * @brief 从完整key路径中提取服务名称
     * @param key etcd中的完整key（格式：/serviceName/instanceId）
     * @return 提取出的serviceName
     */
    std::string ParseKey(const std::string& key);

private:
    std::string _etcdHost;                          ///< etcd服务器地址
    ModifyCallback _onlineCallback;                 ///< 服务上线回调函数
    ModifyCallback _offlineCallback;                ///< 服务下线回调函数
    std::shared_ptr<etcd::Watcher> _watcher;        ///< etcd监视器，持续监听服务变化
};

} // namespace viewEtcd