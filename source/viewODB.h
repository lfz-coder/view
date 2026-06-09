/**
 * @file viewODB.h
 * @brief ODB 数据库模块 —— 基于 ODB 的 MySQL 数据库连接工厂封装
 * @author Your Name
 * @date 2026
 *
 * 该模块封装了 ODB 库，提供 MySQL 数据库连接配置结构体和连接工厂类。
 * 返回 odb::database 基类指针，调用方可通过 ODB 持久化 API 进行数据库操作。
 *
 * 使用示例：
 * @code
 * viewODB::MysqlSettings settings;
 * settings.host     = "127.0.0.1";
 * settings.user     = "root";
 * settings.password = "your_password";
 * settings.database = "your_database";
 *
 * auto db = viewODB::DatabaseFactory::Mysql(settings);
 * if (db) {
 *     odb::transaction t(db->begin());
 *     // 使用 odb::persist / odb::load / odb::query 进行数据库操作
 *     t.commit();
 * }
 * @endcode
 */

#pragma once
#include <odb/database.hxx>
#include <odb/mysql/transaction.hxx>
#include <odb/mysql/database.hxx>

namespace viewODB {

/**
 * @struct MysqlSettings
 * @brief MySQL 数据库连接配置参数结构体
 *
 * 用于配置 MySQL 数据库的连接参数，包括服务器地址、端口、
 * 认证信息、数据库名称、字符集和连接池大小等。
 *
 * 参数说明：
 * - host、user、password、database 为必填项
 * - connectPoolSize 控制底层连接池大小，建议根据并发量调整
 */
struct MysqlSettings {
    std::string host;                        ///< MySQL 服务器地址（IP 或域名），必填
    std::string user;                        ///< MySQL 用户名，必填
    std::string password;                    ///< MySQL 密码，必填
    std::string database;                    ///< 数据库名称，必填
    std::string cset = "utf8";               ///< 字符集，默认 "utf8"
    unsigned int port = 3306;                ///< MySQL 服务器端口，默认 3306
    unsigned int connectPoolSize = 3;        ///< 连接池大小，默认 3，建议按并发量调整
};

/**
 * @class DatabaseFactory
 * @brief ODB 数据库连接工厂类
 *
 * 提供静态工厂方法 Mysql()，根据 MysqlSettings 配置创建 odb::database 连接实例。
 * 内部使用 ODB 的 MySQL 连接池机制，支持多线程并发访问。
 *
 * 线程安全说明：odb::database 对象本身支持多线程访问，
 * 多个线程可以共享同一个 database 实例。
 *
 * 生命周期说明：返回的 std::shared_ptr<odb::database> 由调用者持有，
 * 当所有引用释放后自动断开连接并销毁连接池。
 */
class DatabaseFactory {
public:
    /**
     * @brief 创建 MySQL 数据库连接实例
     * @param settings MySQL 连接配置参数（输入型参数，使用常量引用）
     * @return odb::database 基类指针，创建失败返回 nullptr
     *
     * 根据 settings 中的 host、port、user、password、database、cset 等参数
     * 构建 MySQL 数据库连接，并配置连接池大小，最终创建并返回一个可用的
     * odb::database 实例。
     *
     * @note 如果 host、user、password、database 任一为空，返回 nullptr 并通过 viewLog 输出错误日志。
     * @note 如果连接创建过程中抛异常，会被捕获并通过 viewLog 输出错误日志。
     *
     * @code
     * viewODB::MysqlSettings settings;
     * settings.host     = "127.0.0.1";
     * settings.user     = "root";
     * settings.password = "your_password";
     * settings.database = "your_database";
     * auto db = viewODB::DatabaseFactory::Mysql(settings);
     * if (db) {
     *     // 使用 db 进行数据库操作
     * }
     * @endcode
     */
    static std::shared_ptr<odb::database> Mysql(const MysqlSettings& settings);
};

} // namespace viewODB
