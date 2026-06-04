/**
 * @file viewLog.h
 * @brief 日志模块 —— 基于 spdlog 的日志封装
 * @author Your Name
 * @date 2026
 *
 * 该模块封装了 spdlog 日志库，提供统一的日志器初始化、带文件行号的宏定义，
 * 支持同步/异步模式、控制台/文件输出、日志级别控制等。
 */

#pragma once
// 主要为了满足 spdlog 的日志的输出行号和文件信息更加好用
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>

namespace viewLog {
    /**
     * @struct log_settings
     * @brief 日志配置参数结构体
     */
    struct log_settings {
        bool async = false;                 ///< 是否启用异步日志
        int level = 1;                      ///< 日志输出等级: 1=debug, 2=info, 3=warn, 4=error, 6=off
        std::string format = "[%H:%M:%S][%-7l]: %v"; ///< 日志输出格式
        std::string path = "stdout";        ///< 日志输出目标， "stdout" 表示控制台，否则为文件路径
    };

    /// 全局日志器，由 init_logger() 初始化
    extern std::shared_ptr<spdlog::logger> g_logger;

    /**
     * @brief 初始化全局日志器
     * @param settings 日志配置参数，默认使用 log_settings() 的默认值
     */
    void init_logger(const log_settings& settings = log_settings());

    // 封装日志输出宏，在每条日志前添加 [文件名:行号] 前缀
    #define FMT_PREFIX std::string("[{}:{}]: ")
    #define DEBUG(fmt, ...)   g_logger->debug(FMT_PREFIX + fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define INFO(fmt, ...)    g_logger->info(FMT_PREFIX + fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define WARN(fmt, ...)    g_logger->warn(FMT_PREFIX + fmt, __FILE__, __LINE__, ##__VA_ARGS__)
    #define ERROR(fmt, ...)   g_logger->error(FMT_PREFIX + fmt, __FILE__, __LINE__, ##__VA_ARGS__)
}