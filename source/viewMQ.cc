/**
 * @file viewMQ.cc
 * @brief 消息队列模块实现 —— MQClient / PublishClient / SubscribeClient
 *
 * 基于 AMQP-CPP 和 libev 事件循环的 RabbitMQ 客户端封装。
 * 实现交换机/队列声明（含死信队列 DLX 支持）、消息发布、消息消费。
 */

#include "viewMQ.h"
#include "viewLog.h"

#include <cstring>

namespace viewMQ {

// ==================== DeclareSetting 实现 ====================

std::string DeclareSetting::DlxExchange() const {
    return "dlx." + exchangeName;
}

std::string DeclareSetting::DlxQueue() const {
    return "dlx." + queueName;
}

std::string DeclareSetting::DlxBindingKey() const {
    return "dlx." + bindingKey;
}

// ==================== Exchange_type 实现 ====================

AMQP::ExchangeType Exchange_type(const std::string& type) {
    if (type == "fanout")  return AMQP::ExchangeType::fanout;
    if (type == "direct")  return AMQP::ExchangeType::direct;
    if (type == "topic")   return AMQP::ExchangeType::topic;
    if (type == "headers") return AMQP::ExchangeType::headers;
    if (type == "delayed") return AMQP::ExchangeType::direct;
    // 默认返回 direct 类型
    viewLog::WARN("Exchange_type: 未知类型 '{}'，fallback to 'direct'", type);
    abort();
}

// ==================== MQClient 实现 ====================

MQClient::MQClient(const std::string& url)
    : _ev_loop(ev_loop_new(EVFLAG_AUTO))
    , _handler(_ev_loop)
    , _connection(nullptr)
    , _channel(nullptr)
{
    if (!_ev_loop) {
        viewLog::ERROR("MQClient: 创建 libev 事件循环失败");
        return;
    }

    // 解析 AMQP 地址并创建连接
    AMQP::Address address(url);
    _connection = new AMQP::TcpConnection(&_handler, address);
    _channel    = new AMQP::TcpChannel(_connection);

    // 初始化异步唤醒器（用于析构时安全退出事件循环）
    ev_async_init(&_ev_async, [](struct ev_loop* loop, ev_async* /*w*/, int /*revents*/) {
        ev_break(loop, EVBREAK_ALL);
    });
    ev_async_start(_ev_loop, &_ev_async);

    viewLog::INFO("MQClient: 初始化完成, url={}", url);
}

MQClient::~MQClient() {
    // 1. 先清理 AMQP 资源（channel/connection 注销其 libev watcher）
    delete _channel;
    _channel = nullptr;
    delete _connection;
    _connection = nullptr;

    // 2. 如果事件循环线程已启动，发送停止信号并等待
    if (_async_thread.joinable()) {
        ev_async_send(_ev_loop, &_ev_async);
        _async_thread.join();
    }

    // 3. 停止 async watcher
    if (_ev_loop) {
        ev_async_stop(_ev_loop, &_ev_async);
    }

    // 注意：不在此处销毁 _ev_loop，因为成员 _handler 在此函数返回后才析构，
    // 其析构函数需要 _ev_loop 仍然有效。_ev_loop 将在进程退出时由 OS 回收。

    viewLog::INFO("MQClient: 资源清理完成");
}

void MQClient::Declare(const DeclareSetting& setting) {
    if (!_channel) {
        viewLog::ERROR("MQClient::Declare: channel 未初始化");
        return;
    }

    AMQP::ExchangeType exType = Exchange_type(setting.exchangeType);

    // ── 如果有 delayTTL，先声明死信队列（DLX） ──
    if (setting.delayTTL > 0) {
        std::string dlxExchange   = setting.DlxExchange();
        std::string dlxQueue      = setting.DlxQueue();
        std::string dlxBindingKey = setting.DlxBindingKey();

        // 声明死信交换机
        _channel->declareExchange(dlxExchange, exType,
            AMQP::durable | AMQP::autodelete);

        // 声明死信队列
        _channel->declareQueue(dlxQueue,
            AMQP::durable | AMQP::autodelete);

        // 绑定死信队列到死信交换机
        _channel->bindQueue(dlxExchange, dlxQueue, dlxBindingKey);

        viewLog::INFO("MQClient::Declare: DLX 声明完成: exchange={}, queue={}",
                      dlxExchange, dlxQueue);
    }

    // ── 声明主交换机 ──
    _channel->declareExchange(setting.exchangeName, exType,
        AMQP::durable | AMQP::autodelete);

    // ── 声明主队列 ──
    AMQP::Table arguments;
    if (setting.delayTTL > 0) {
        arguments["x-dead-letter-exchange"] = setting.DlxExchange();
        arguments["x-message-ttl"]          = static_cast<uint64_t>(setting.delayTTL);
        arguments["x-dead-letter-routing-key"] = setting.DlxBindingKey();
    }

    _channel->declareQueue(setting.queueName,
        AMQP::durable | AMQP::autodelete, arguments);

    // ── 绑定队列到交换机 ──
    _channel->bindQueue(setting.exchangeName, setting.queueName,
        setting.bindingKey);

    viewLog::INFO("MQClient::Declare: exchange={}, queue={}, bindingKey={}, delayTTL={}",
                  setting.exchangeName, setting.queueName, setting.bindingKey, setting.delayTTL);
}

void MQClient::Publish(const std::string& exchange,
                        const std::string& routingKey,
                        const std::string& message) {
    if (!_channel) {
        viewLog::ERROR("MQClient::Publish: channel 未初始化");
        return;
    }

    _channel->publish(exchange, routingKey, message);
    viewLog::DEBUG("MQClient::Publish: exchange={}, routingKey={}, size={}",
                   exchange, routingKey, message.size());
}

void MQClient::Consume(const std::string& queue, const MessageCallback& callback) {
    if (!_channel) {
        viewLog::ERROR("MQClient::Consume: channel 未初始化");
        return;
    }

    _channel->consume(queue)
        .onReceived([callback](const AMQP::Message& msg,
                                uint64_t /*deliveryTag*/,
                                bool /*redelivered*/) {
            if (callback) {
                callback(msg.body(), msg.bodySize());
            }
        })
        .onError([queue](const char* message) {
            viewLog::ERROR("MQClient::Consume: 消费错误, queue={}, error={}",
                           queue, message);
        });

    viewLog::INFO("MQClient::Consume: queue={} 开始消费", queue);
}

void MQClient::Start() {
    if (!_ev_loop) {
        viewLog::ERROR("MQClient::Start: 事件循环未初始化");
        return;
    }

    _async_thread = std::thread([this]() {
        viewLog::INFO("MQClient: 事件循环线程启动");
        ev_run(_ev_loop, 0);
        viewLog::INFO("MQClient: 事件循环线程退出");
    });
}

void MQClient::Wait() {
    if (_async_thread.joinable()) {
        _async_thread.join();
    }
}

// ==================== PublishClient 实现 ====================

PublishClient::PublishClient(MQClient::Ptr mqClient,
                               const DeclareSetting& declareSetting)
    : _mqClient(std::move(mqClient))
    , _declareSetting(declareSetting)
{
    if (_mqClient) {
        _mqClient->Declare(_declareSetting);
    }
}

void PublishClient::Publish(const std::string& message) {
    if (!_mqClient) {
        viewLog::ERROR("PublishClient::Publish: MQClient 无效");
        return;
    }
    _mqClient->Publish(_declareSetting.exchangeName,
                       _declareSetting.bindingKey,
                       message);
}

// ==================== SubscribeClient 实现 ====================

SubscribeClient::SubscribeClient(MQClient::Ptr mqClient,
                                   const DeclareSetting& declareSetting)
    : _mqClient(std::move(mqClient))
    , _declareSetting(declareSetting)
{
    if (_mqClient) {
        _mqClient->Declare(_declareSetting);
    }
}

void SubscribeClient::Consume(const MessageCallback& callback) {
    if (!_mqClient) {
        viewLog::ERROR("SubscribeClient::Consume: MQClient 无效");
        return;
    }
    _mqClient->Consume(_declareSetting.queueName, callback);
}

} // namespace viewMQ
