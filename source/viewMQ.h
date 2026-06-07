/**
 * @file viewMQ.h
 * @brief 消息队列模块 —— 基于 AMQP-CPP + libev 的 RabbitMQ 客户端封装
 * @author Your Name
 * @date 2026
 *
 * 提供交换机/队列声明（含死信队列 DLX 支持）、消息发布、消息消费功能。
 * 包含以下核心类型：
 * - DeclareSetting：AMQP 交换机/队列声明配置
 * - MQClient：      核心客户端（libev 事件循环）
 * - PublishClient：  发布客户端（组合模式）
 * - SubscribeClient：订阅客户端（组合模式）
 * - MQFactory：      模板工厂类
 */

#pragma once

#include <amqpcpp.h>
#include <ev.h>
#include <amqpcpp/libev.h>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace viewMQ {

/**
 * @struct DeclareSetting
 * @brief AMQP 交换机与队列声明配置
 *
 * 封装了一次交换机 + 队列 + 绑定关系的声明参数，
 * 并内置了死信队列（DLX, Dead Letter Exchange）命名规则的支持。
 *
 * 使用方式：
 *   1. 填充 exchangeName / exchangeType / queueName / bindingKey / delayTTL
 *   2. 调用 DlxExchange() / DlxQueue() / DlxBindingKey() 获取对应的 DLX 名称
 *   3. 将配置传递给 AMQP 通道进行实际声明
 */
struct DeclareSetting {
    // ──────────────── 基础声明参数 ────────────────

    /// 交换机名称
    std::string exchangeName;

    /// 交换机类型，如 "direct" / "fanout" / "topic" / "headers"
    std::string exchangeType;

    /// 队列名称
    std::string queueName;

    /// 绑定路由键（Binding Key），队列与交换机绑定时使用
    std::string bindingKey;

    /// 消息延迟 / 超时时间，单位：毫秒
    size_t delayTTL;

    // ──────────────── 死信队列（DLX）相关 ────────────────

    /**
     * @brief 获取死信交换机名称
     * @return 格式为 "dlx.{exchangeName}" 的字符串
     */
    std::string DlxExchange() const;

    /**
     * @brief 获取死信队列名称
     * @return 格式为 "dlx.{queueName}" 的字符串
     */
    std::string DlxQueue() const;

    /**
     * @brief 获取死信队列绑定键
     * @return 格式为 "dlx.{bindingKey}" 的字符串
     */
    std::string DlxBindingKey() const;
};

extern AMQP::ExchangeType Exchange_type(const std::string& type);
/// 消息回调函数类型：参数为消息体指针和长度
using MessageCallback = std::function<void(const char*, size_t)>;

/**
 * @class MQClient
 * @brief AMQP/RabbitMQ 核心客户端类
 *
 * 基于 AMQP-CPP 和 libev 事件循环的 RabbitMQ 客户端封装。
 * 内部启动独立线程运行 libev 事件循环，支持声明、发布、消费操作。
 *
 * 使用流程：
 * 1. 构造 MQClient(url) → 初始化连接和事件循环
 * 2. Declare(setting) → 声明交换机/队列/绑定
 * 3. Start() → 启动事件循环线程
 * 4. Publish() / Consume() → 收发消息
 * 5. 析构时自动停止事件循环并清理资源
 */
class MQClient {
public:
    using Ptr = std::shared_ptr<MQClient>; ///< MQClient 智能指针类型别名

    /**
     * @brief 构造 MQClient 并初始化 AMQP 连接
     * @param url AMQP 连接地址（格式：amqp://user:pass@host:port/vhost）
     */
    MQClient(const std::string& url);

    /**
     * @brief 析构函数——清理 AMQP 资源，停止事件循环，等待线程退出
     */
    ~MQClient();

    /**
     * @brief 声明交换机、队列及绑定关系，支持死信队列（DLX）配置
     * @param setting 声明配置参数（包含交换机名、类型、队列名、绑定键、TTL 等）
     */
    void Declare(const DeclareSetting& setting);

    /**
     * @brief 发布消息到指定交换机和路由键
     * @param exchange   目标交换机名称
     * @param routingKey 路由键
     * @param message    消息体
     */
    void Publish(const std::string& exchange, const std::string& routingKey, const std::string& message);

    /**
     * @brief 从指定队列消费消息
     * @param queue    队列名称
     * @param callback 消息处理回调函数
     */
    void Consume(const std::string& queue, const MessageCallback& callback);

    /**
     * @brief 启动 libev 事件循环（在独立线程中运行）
     */
    void Start();

    /**
     * @brief 等待事件循环线程结束（阻塞调用）
     */
    void Wait();
private:
    std::mutex _mtx;
    std::condition_variable _cv;
    struct ev_loop* _ev_loop;
    struct ev_async _ev_async;
    AMQP::LibEvHandler _handler;
    AMQP::TcpConnection* _connection;
    AMQP::TcpChannel* _channel;
    std::thread _async_thread; // 异步处理事件循环
};

/**
 * @class PublishClient
 * @brief 消息发布客户端 —— 组合 MQClient，预设交换机和声明配置，简化发布操作
 */
class PublishClient {
public:
    using Ptr = std::shared_ptr<PublishClient>;

    /**
     * @brief 构造函数
     * @param mqClient 已初始化的 MQClient 实例
     * @param declareSetting 交换机/队列声明配置
     */
    PublishClient(MQClient::Ptr mqClient, const DeclareSetting& declareSetting);

    /**
     * @brief 发布消息到预设的交换机
     * @param message 消息体
     */
    void Publish(const std::string& message);

private:
    MQClient::Ptr _mqClient;          ///< 内部使用的 MQClient 实例
    DeclareSetting _declareSetting;   ///< 发布客户端的声明配置
};

/**
 * @class SubscribeClient
 * @brief 消息订阅客户端 —— 组合 MQClient，预设队列和声明配置，简化消费操作
 */
class SubscribeClient {
public:
    using Ptr = std::shared_ptr<SubscribeClient>;

    /**
     * @brief 构造函数
     * @param mqClient 已初始化的 MQClient 实例
     * @param declareSetting 交换机/队列声明配置
     */
    SubscribeClient(MQClient::Ptr mqClient, const DeclareSetting& declareSetting);

    /**
     * @brief 从预设队列消费消息
     * @param callback 消息处理回调函数
     */
    void Consume(const MessageCallback& callback);

private:
    MQClient::Ptr _mqClient;          ///< 内部使用的 MQClient 实例
    DeclareSetting _declareSetting;   ///< 订阅客户端的声明配置
};

/**
 * @class MQFactory
 * @brief MQ 客户端模板工厂类
 *
 * 提供泛型工厂方法 Create<T>()，使用 std::make_shared + 完美转发构造派生客户端类。
 *
 * @example
 * @code
 * auto pub = MQFactory::Create<PublishClient>(mqClient, setting);
 * @endcode
 */
class MQFactory {
public:
    /**
     * @brief 模板工厂方法——创建指定类型的客户端实例
     * @tparam T    目标客户端类型（如 PublishClient / SubscribeClient）
     * @tparam Args 构造函数参数类型
     * @param args  转发给 T 构造函数的参数
     * @return std::shared_ptr<T> 创建的实例
     */
    template<typename T, typename... Args>
    static std::shared_ptr<T> Create(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

} // namespace viewMQ
