#include <viewRedis.h>
#include <viewLog.h>

int main() {
    viewLog::init_logger();
    try {
        viewRedis::RedisSettings settings = {
            .host = "192.168.10.129",
            .password = "123456",
            .connectionPoolSize = 3
        };

        // ==================== 关键说明 ====================
        // 1. 本工厂创建的 redis 对象已启用【连接池】
        // 2. 带连接池的 sw::redis::Redis 对象是【线程安全】的
        // 3. 多线程可直接共用，不存在线程安全问题
        auto redis = viewRedis::RedisFactory::Create(settings);

        {
            // ==================== 事务使用说明（正确版） ====================
            // 此处创建事务对象，目的是：
            // 1. 将多个命令打包批量执行（提升性能）
            // 2. 保证多个命令的【原子性】：要么全部执行，要么全部不执行
            //
            // 注意：
            // - 事务对象【非线程安全】，请勿跨线程共享
            // - 事务不是为了解决线程安全，而是保证命令原子执行
            // - 普通单命令操作（单独 set / get）无需使用事务
            auto tx = redis->transaction(false, false);

            // 从事务中获取操作对象，用于执行 Redis 命令
            auto redisTx = tx.redis();

            // 以下命令会先入队，不会立即执行
            bool ret = redisTx.set("key1", "value1");
            if (ret) {
                viewLog::INFO("set key1 success");
            } else {
                viewLog::ERROR("set key1 failed");
            }

            auto val = redisTx.get("key1");
            if (val) {
                viewLog::INFO("get key1 success, value: {}", *val);
            } else {
                viewLog::ERROR("get key1 failed");
            }

            // 刷新缓冲区，执行事务队列中的所有命令（原子执行）
            redisTx.flushall();
        }

    } catch(const sw::redis::Error& err) {
        viewLog::ERROR("Redis error: {}", err.what());
    } catch(const std::exception& ex) {
        viewLog::ERROR("Exception: {}", ex.what());
    }
    return 0;
}