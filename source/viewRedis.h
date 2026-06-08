/**
 * @file viewRedis.h
 * @brief Redis 客户端模块 —— 基于 redis-plus-plus 的连接工厂封装
 * @author Your Name
 * @date 2026
 *
 * 该模块封装了 redis-plus-plus 库，提供 Redis 连接配置结构体和连接工厂类。
 * 支持连接池管理、密码认证、数据库选择等常用配置。
 *
 * 使用示例：
 * @code
 * viewRedis::RedisSettings settings;
 * settings.host     = "127.0.0.1";
 * settings.port     = 6379;
 * settings.password = "your_password";
 * settings.db       = 0;
 *
 * auto redis = viewRedis::RedisFactory::Create(settings);
 * if (redis) {
 *     redis->set("key", "value");
 *     auto val = redis->get("key");
 * }
 * @endcode
 */

#pragma once
#include <sw/redis++/redis.h>
#include <sw/redis++/queued_redis.h>
#include <iostream>
#include <chrono>
#include <unordered_set>

namespace viewRedis {

/**
 * @struct RedisSettings
 * @brief Redis 连接配置参数结构体
 *
 * 用于配置 Redis 客户端的连接参数，包括服务器地址、端口、
 * 认证信息、数据库编号和连接池大小等。
 *
 * 参数说明：
 * - host 为必填项，其他字段均有合理默认值
 * - connectionPoolSize 控制底层连接池大小，建议根据并发量调整
 */
struct RedisSettings {
    int db = 0;                                ///< Redis 数据库编号（0-15），默认 0
    int port = 6379;                           ///< Redis 服务器端口，默认 6379
    std::string host;                          ///< Redis 服务器地址（IP 或域名），必填
    std::string user = "default";              ///< Redis ACL 用户名，默认 "default"（Redis 6.0+）
    std::string password;                      ///< Redis 密码，空表示无密码认证
    size_t connectionPoolSize = 3;             ///< 连接池大小，默认 3，建议按并发量调整
};

/**
 * @class RedisFactory
 * @brief Redis 连接工厂类
 *
 * 提供静态工厂方法 Create()，根据 RedisSettings 配置创建 sw::redis::Redis 连接实例。
 * 内部使用 redis-plus-plus 的连接池机制，支持多线程并发访问。
 *
 * 线程安全说明：sw::redis::Redis 对象本身是线程安全的，
 * 多个线程可以共享同一个 Redis 实例。
 *
 * 生命周期说明：返回的 std::shared_ptr<sw::redis::Redis> 由调用者持有，
 * 当所有引用释放后自动断开连接并销毁连接池。
 */
class RedisFactory {
public:
    /**
     * @brief 创建 Redis 连接实例
     * @param settings Redis 连接配置参数（输入型参数，使用常量引用）
     * @return Redis 连接实例的智能指针，创建失败返回 nullptr
     *
     * 根据 settings 中的 host、port、db、user、password 等参数构建连接选项，
     * 并配置连接池大小，最终创建并返回一个可用的 Redis 连接实例。
     *
     * @note 如果 host 为空，返回 nullptr 并通过 viewLog 输出错误日志。
     * @note 如果连接创建过程中抛异常，会被捕获并通过 viewLog 输出错误日志。
     *
     * @code
     * viewRedis::RedisSettings settings;
     * settings.host = "127.0.0.1";
     * auto redis = viewRedis::RedisFactory::Create(settings);
     * if (redis) {
     *     // 使用 redis 进行读写操作
     * }
     * @endcode
     */
    static std::shared_ptr<sw::redis::Redis> Create(const RedisSettings& settings);
};

} // namespace viewRedis
