/**
 * @file viewMQ.cc
 * @brief 消息队列模块实现 —— MQClient / PublishClient / SubscribeClient
 * @warning 本模块尚未实现，请勿在项目中使用
 */

#include "viewMQ.h"

#error "viewMQ 模块尚未实现，请先完成以下类的实现：MQClient, PublishClient, SubscribeClient"

namespace viewMQ {

// TODO: 实现 MQClient 类
// - 构造函数：创建 libev 事件循环、AMQP 连接和 Channel
// - 析构函数：关闭事件循环、等待异步线程结束、清理资源
// - Declare()：声明交换机、队列、绑定关系
// - Publish()：发布消息到指定交换机
// - Consume()：消费消息，通过回调传递消息体
// - Start()/Wait()：启动/等待事件循环

// TODO: 实现 PublishClient / SubscribeClient
// - 应使用组合模式（持有 MQClient::Ptr），去除当前的双重继承+组合设计

} // namespace viewMQ
