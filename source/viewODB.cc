/**
 * @file viewODB.cc
 * @brief ODB 数据库模块实现 —— MySQL 数据库连接工厂
 *
 * 基于 ODB 的 MySQL 数据库连接创建，封装连接选项配置、连接池管理和错误处理。
 */

#include "viewODB.h"
#include "viewLog.h"

#include <odb/mysql/connection-factory.hxx>

namespace viewODB {

// ==================== DatabaseFactory::Mysql 实现 ====================

/**
 * @brief 根据 MysqlSettings 配置创建 MySQL 数据库连接实例
 * @param settings MySQL 连接配置参数
 * @return odb::database 基类指针，创建失败返回 nullptr
 *
 * 创建流程：
 * 1. 校验必填项 host、user、password、database 是否为空
 * 2. 构建 odb::mysql::connection_pool_factory（连接池管理）
 * 3. 创建 odb::mysql::database 实例并返回其 odb::database 基类指针
 *
 * 异常处理：捕获 std::exception 并通过 viewLog 输出错误日志，
 * 返回 nullptr 表示创建失败。
 */
std::shared_ptr<odb::database> DatabaseFactory::Mysql(const MysqlSettings& settings) {
    // ===================== 1. 参数校验 =====================
    if (settings.host.empty()) {
        viewLog::ERROR("DatabaseFactory::Mysql 失败: host 为空，MySQL 服务器地址为必填项");
        return nullptr;
    }

    if (settings.user.empty()) {
        viewLog::ERROR("DatabaseFactory::Mysql 失败: user 为空，数据库用户名为必填项");
        return nullptr;
    }

    if (settings.password.empty()) {
        viewLog::ERROR("DatabaseFactory::Mysql 失败: password 为空，数据库密码为必填项");
        return nullptr;
    }

    if (settings.database.empty()) {
        viewLog::ERROR("DatabaseFactory::Mysql 失败: database 为空，数据库名称为必填项");
        return nullptr;
    }

    // ===================== 2. 构建连接池工厂 =====================
    auto poolFactory = std::make_unique<odb::mysql::connection_pool_factory>(
        settings.connectPoolSize);

    // ===================== 3. 创建 MySQL 数据库连接实例 =====================
    try {
        auto db = std::make_shared<odb::mysql::database>(
            settings.user,
            settings.password,
            settings.database,
            settings.host,
            settings.port,
            nullptr,          // socket，使用默认值
            settings.cset,
            0,                // client_flags，使用默认值
            std::move(poolFactory));

        viewLog::INFO("DatabaseFactory::Mysql 成功: host={}, port={}, "
                       "database={}, cset={}, poolSize={}",
                       settings.host, settings.port,
                       settings.database, settings.cset,
                       settings.connectPoolSize);
        return db;
    } catch (const std::exception& e) {
        viewLog::ERROR("DatabaseFactory::Mysql 失败: 创建 MySQL 数据库连接异常, "
                        "host={}, port={}, database={}, error={}",
                        settings.host, settings.port, settings.database, e.what());
        return nullptr;
    }
}

} // namespace viewODB
