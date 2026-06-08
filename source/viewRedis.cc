/**
 * @file viewRedis.cc
 * @brief Redis 客户端模块实现 —— Redis 连接工厂
 *
 * 基于 redis-plus-plus 的 Redis 连接创建，封装连接选项配置和错误处理。
 */

#include "viewRedis.h"
#include "viewLog.h"

namespace viewRedis {

// ==================== RedisFactory::Create 实现 ====================

/**
 * @brief 根据 RedisSettings 配置创建 Redis 连接实例
 * @param settings Redis 连接配置参数
 * @return Redis 连接实例的智能指针，创建失败返回 nullptr
 *
 * 创建流程：
 * 1. 校验 host 是否为空
 * 2. 构建 sw::redis::ConnectionOptions（host、port、db、user、password）
 * 3. 构建 sw::redis::ConnectionPoolOptions（连接池大小）
 * 4. 创建 sw::redis::Redis 实例并通过 ping 验证连接可用性
 *
 * 异常处理：捕获 std::exception 并通过 viewLog 输出错误日志，
 * 返回 nullptr 表示创建失败。
 */
std::shared_ptr<sw::redis::Redis> RedisFactory::Create(const RedisSettings& settings) {
    // ===================== 1. 参数校验 =====================
    if (settings.host.empty()) {
        viewLog::ERROR("RedisFactory::Create 失败: host 为空，Redis 服务器地址为必填项");
        return nullptr;
    }

    // ===================== 2. 构建连接选项 =====================
    sw::redis::ConnectionOptions connOpts;
    connOpts.host     = settings.host;
    connOpts.port     = settings.port;
    connOpts.db       = settings.db;
    connOpts.user     = settings.user;
    connOpts.password = settings.password;

    // 设置 socket 超时，避免长时间阻塞
    connOpts.socket_timeout = std::chrono::milliseconds(3000);

    // ===================== 3. 构建连接池选项 =====================
    sw::redis::ConnectionPoolOptions poolOpts;
    poolOpts.size = settings.connectionPoolSize;

    // ===================== 4. 创建 Redis 连接实例 =====================
    try {
        auto redis = std::make_shared<sw::redis::Redis>(connOpts, poolOpts);

        // 连接创建后执行 ping 验证连接可用性
        std::string pingResult = redis->ping();
        viewLog::INFO("RedisFactory::Create 成功: host={}, port={}, db={}, "
                       "poolSize={}, ping={}",
                       settings.host, settings.port, settings.db,
                       settings.connectionPoolSize, pingResult);
        return redis;
    } catch (const std::exception& e) {
        viewLog::ERROR("RedisFactory::Create 失败: 创建 Redis 连接异常, "
                        "host={}, port={}, error={}",
                        settings.host, settings.port, e.what());
        return nullptr;
    }
}

} // namespace viewRedis
