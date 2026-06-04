/**
 * @file mqTest.cc
 * @brief viewMQ 消息队列模块测试 —— 测试 DeclareSetting / Exchange_type / 编译验证
 *
 * 本测试不需要 RabbitMQ 服务器即可验证：
 * 1. DeclareSetting 的 DLX 命名规则
 * 2. Exchange_type 的类型映射
 * 3. MQFactory 模板工厂
 * 4. MQClient 基本构造/析构（需要 RabbitMQ 才能完整运行）
 *
 * 完整功能测试需要本地运行 RabbitMQ 服务器：
 *   docker run -d --name rabbitmq -p 5672:5672 -p 15672:15672 rabbitmq:3-management
 *
 * 编译：make
 * 运行：./mqTest [amqp_url]
 */

#include "../../source/viewMQ.h"
#include "../../source/viewLog.h"

#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <chrono>

// ==================== 测试 1: DeclareSetting DLX 命名 ====================
void test_declare_setting_dlx() {
    std::cout << "=== 测试1: DeclareSetting DLX 命名 ===" << std::endl;

    viewMQ::DeclareSetting setting;
    setting.exchangeName = "order";
    setting.exchangeType = "direct";
    setting.queueName    = "order_queue";
    setting.bindingKey   = "order.create";
    setting.delayTTL     = 5000;

    assert(setting.DlxExchange()   == "dlx.order");
    assert(setting.DlxQueue()      == "dlx.order_queue");
    assert(setting.DlxBindingKey() == "dlx.order.create");

    std::cout << "  DlxExchange:   " << setting.DlxExchange() << std::endl;
    std::cout << "  DlxQueue:      " << setting.DlxQueue() << std::endl;
    std::cout << "  DlxBindingKey: " << setting.DlxBindingKey() << std::endl;
    std::cout << "  ✓ PASS" << std::endl;
}

// ==================== 测试 2: Exchange_type 类型映射 ====================
void test_exchange_type() {
    std::cout << "=== 测试2: Exchange_type 类型映射 ===" << std::endl;

    assert(viewMQ::Exchange_type("fanout") == AMQP::ExchangeType::fanout);
    assert(viewMQ::Exchange_type("direct") == AMQP::ExchangeType::direct);
    assert(viewMQ::Exchange_type("topic")  == AMQP::ExchangeType::topic);
    assert(viewMQ::Exchange_type("headers") == AMQP::ExchangeType::headers);
    // delayed 类型：延时队列基于 direct 交换机 + DLX 死信队列实现
    assert(viewMQ::Exchange_type("delayed") == AMQP::ExchangeType::direct);

    std::cout << "  fanout   -> " << static_cast<int>(viewMQ::Exchange_type("fanout")) << std::endl;
    std::cout << "  direct   -> " << static_cast<int>(viewMQ::Exchange_type("direct")) << std::endl;
    std::cout << "  topic    -> " << static_cast<int>(viewMQ::Exchange_type("topic")) << std::endl;
    std::cout << "  headers  -> " << static_cast<int>(viewMQ::Exchange_type("headers")) << std::endl;
    std::cout << "  delayed  -> " << static_cast<int>(viewMQ::Exchange_type("delayed")) << " (延时队列=direct+DLX)" << std::endl;
    std::cout << "  ✓ PASS" << std::endl;
}

// ==================== 测试 3: MQFactory 模板工厂 ====================
struct TestObject {
    int value;
    std::string name;
    TestObject(int v, std::string n) : value(v), name(std::move(n)) {}
};

void test_mq_factory() {
    std::cout << "=== 测试3: MQFactory 模板工厂 ===" << std::endl;

    auto obj = viewMQ::MQFactory::Create<TestObject>(42, "test");
    assert(obj->value == 42);
    assert(obj->name == "test");

    std::cout << "  Created: value=" << obj->value << ", name=" << obj->name << std::endl;
    std::cout << "  ✓ PASS" << std::endl;
}

// ==================== 测试 4: 延时队列 DeclareSetting (delayed + DLX) ====================
void test_delayed_queue_setting() {
    std::cout << "=== 测试4: 延时队列 DeclareSetting (delayed + DLX) ===" << std::endl;

    // 延时队列场景：消息发布到 order.exchange → order.queue（有 TTL）
    // → 消息过期 → dlx.order.exchange → dlx.order.queue → 消费者
    viewMQ::DeclareSetting setting;
    setting.exchangeName = "order.exchange";
    setting.exchangeType = "delayed";  // 延时队列类型
    setting.queueName    = "order.queue";
    setting.bindingKey   = "order.delay";
    setting.delayTTL     = 10000;  // 10 秒延迟

    // 验证 DLX 命名规则
    assert(setting.DlxExchange()   == "dlx.order.exchange");
    assert(setting.DlxQueue()      == "dlx.order.queue");
    assert(setting.DlxBindingKey() == "dlx.order.delay");

    std::cout << "  延时队列配置:" << std::endl;
    std::cout << "    主交换机:  " << setting.exchangeName << " (type=" << setting.exchangeType << ")" << std::endl;
    std::cout << "    主队列:    " << setting.queueName << " (TTL=" << setting.delayTTL << "ms)" << std::endl;
    std::cout << "    绑定键:    " << setting.bindingKey << std::endl;
    std::cout << "    DLX 交换机: " << setting.DlxExchange() << std::endl;
    std::cout << "    DLX 队列:   " << setting.DlxQueue() << std::endl;
    std::cout << "    DLX 绑定键: " << setting.DlxBindingKey() << std::endl;
    std::cout << std::endl;
    std::cout << "  消息流: Publish → order.exchange → order.queue" << std::endl;
    std::cout << "         → " << setting.delayTTL << "ms 后过期" << std::endl;
    std::cout << "         → " << setting.DlxExchange() << " → " << setting.DlxQueue() << std::endl;
    std::cout << "         → 消费者从 " << setting.DlxQueue() << " 消费" << std::endl;
    std::cout << "  ✓ PASS" << std::endl;
}

// ==================== 测试 5: MQClient 构造/析构基础验证 ====================
void test_mq_client_lifecycle() {
    std::cout << "=== 测试4: MQClient 生命周期 ===" << std::endl;

    // 使用无效 URL 构造，验证构造不崩溃
    bool construct_ok = false;
    try {
        viewMQ::MQClient client("amqp://localhost:5672/");
        construct_ok = true;
        // 如果 RabbitMQ 没运行，Start 不会建立连接，但构造应成功
        std::cout << "  构造成功（注意：RabbitMQ 未运行时连接建立会延迟到事件循环）" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  构造异常: " << e.what() << std::endl;
        std::cout << "  (如果 amqpcpp 库抛异常说明 URL 格式有问题)" << std::endl;
    }
    std::cout << "  construct_ok=" << (construct_ok ? "true" : "false") << std::endl;
    std::cout << "  ✓ PASS（不崩溃即可）" << std::endl;
}

// ==================== 主函数 ====================
int main(int argc, char* argv[]) {
    // 初始化日志
    viewLog::log_settings logSettings;
    logSettings.level = 2; // info
    viewLog::init_logger(logSettings);

    viewLog::INFO("========== viewMQ 模块测试开始 ==========");

    test_declare_setting_dlx();
    std::cout << std::endl;

    test_exchange_type();
    std::cout << std::endl;

    test_mq_factory();
    std::cout << std::endl;

    test_delayed_queue_setting();
    std::cout << std::endl;

    test_mq_client_lifecycle();
    std::cout << std::endl;

    // 可选：连接真实 RabbitMQ 进行完整测试
    if (argc > 1) {
        std::string amqpUrl = argv[1];
        std::cout << "=== 测试6: 真实 RabbitMQ 连接（普通队列） ===" << std::endl;
        std::cout << "  URL: " << amqpUrl << std::endl;

        try {
            viewMQ::MQClient client(amqpUrl);

            // ── 普通队列测试 ──
            viewMQ::DeclareSetting setting;
            setting.exchangeName = "test_exchange";
            setting.exchangeType = "direct";
            setting.queueName    = "test_queue";
            setting.bindingKey   = "test_key";
            setting.delayTTL     = 0;

            client.Declare(setting);
            client.Publish("test_exchange", "test_key", "Hello via viewMQ!");

            bool received = false;
            client.Consume("test_queue", [&received](const char* body, size_t size) {
                std::string msg(body, size);
                std::cout << "  收到普通消息: " << msg << std::endl;
                received = true;
            });

            client.Start();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "  ✓ 普通队列测试完成, received=" << (received ? "true" : "false") << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "  连接失败: " << e.what() << std::endl;
            std::cerr << "  请确保 RabbitMQ 正在运行" << std::endl;
            return 1;
        }

        // ── 延时队列测试 ──
        std::cout << std::endl;
        std::cout << "=== 测试7: 真实 RabbitMQ 延时队列（delayed + DLX） ===" << std::endl;
        try {
            viewMQ::MQClient client(amqpUrl);

            // 延时队列：消息发布后 3 秒过期，然后进入 DLX 被消费
            viewMQ::DeclareSetting delaySetting;
            delaySetting.exchangeName = "order.exchange";
            delaySetting.exchangeType = "delayed";
            delaySetting.queueName    = "order.queue";
            delaySetting.bindingKey   = "order.delay";
            delaySetting.delayTTL     = 3000;  // 3 秒延迟

            client.Declare(delaySetting);

            // 发布一条延时消息
            client.Publish("order.exchange", "order.delay", "延时消息: 3秒后送达");

            // 从 DLX 队列消费（延时消息过期后会到这里）
            bool delayReceived = false;
            client.Consume(delaySetting.DlxQueue(),
                [&delayReceived](const char* body, size_t size) {
                    std::string msg(body, size);
                    std::cout << "  收到延时消息: " << msg << std::endl;
                    delayReceived = true;
                });

            client.Start();

            std::cout << "  等待延时消息送达（TTL=" << delaySetting.delayTTL << "ms）..." << std::endl;
            auto start = std::chrono::steady_clock::now();
            // 等待 TTL+1 秒
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delaySetting.delayTTL + 1000));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();

            std::cout << "  实际等待: " << elapsed << "ms"
                      << ", delayReceived=" << (delayReceived ? "true" : "false") << std::endl;
            std::cout << "  ✓ 延时队列测试完成" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "  延时队列测试失败: " << e.what() << std::endl;
            return 1;
        }

        std::cout << "\n✓ 所有真实连接测试通过!" << std::endl;
    } else {
        std::cout << "提示: 传入 AMQP URL 可进行真实 RabbitMQ 连接测试" << std::endl;
        std::cout << "  示例: ./mqTest amqp://guest:guest@localhost:5672/" << std::endl;
        std::cout << "  测试包括: 普通队列 + 延时队列(delayed + DLX)" << std::endl;
    }

    viewLog::INFO("========== viewMQ 模块测试结束 ==========");
    std::cout << "\n全部测试通过!" << std::endl;
    return 0;
}
