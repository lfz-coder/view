#pragma once

/**
 * @file viewMQ.h
 * @brief 消息队列声明配置模块
 *
 * 基于 AMQP-CPP 和 libev 事件循环的 RabbitMQ 客户端封装。
 * 提供交换机、队列、死信队列（DLX）的声明配置结构。
 */

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
using MessageCallback = std::function<void(const char*, size_t)>; // 消息回调函数类型定义
class MQClient {
public:
    using Ptr = std::shared_ptr<MQClient>;
    MQClient(const std::string& url); // 构造成员，启动异步事件循环
    ~MQClient(); // 发送异步请求，结束事件循环，等待异步线程结束
    // 声明交换机、队列、绑定关系，支持死信队列配置
    void Declare(const DeclareSetting& setting); 
    void Publish(const std::string& exchange, const std::string& routingKey, const std::string& message); // 发布消息到指定交换机和路由键
    void Consume(const std::string& queue, const MessageCallback& callback); // 从指定队列消费消息，回调函数处理消息内容
    void Start();
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

class MQFactory {
public:
    template<typename T, typename... Args>
    static std::shared_ptr<T> Create(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

} // namespace viewMQ
