// 引入自定义的日志头文件
#include "viewLog.h"
// 引入标准输入输出流，用于打印初始化失败信息
#include <iostream>

// 引入spdlog异步日志核心支持
#include <spdlog/async.h>
// 引入spdlog控制台彩色输出sink
#include <spdlog/sinks/stdout_color_sinks.h>
// 引入spdlog基础文件输出sink
#include <spdlog/sinks/basic_file_sink.h>
// 引入spdlog旋转文件输出sink（按大小切割）
#include <spdlog/sinks/rotating_file_sink.h>

// 自定义命名空间 viewLog，避免全局命名冲突
namespace viewLog {
    // 定义全局日志器智能指针
    // 整个项目都可以通过 view::g_logger 使用日志功能
    std::shared_ptr<spdlog::logger> g_logger;

    /**
     * @brief 日志器初始化接口实现
     * @param settings 日志配置参数（异步/同步、路径、级别、格式等）
     */
    void init_logger(const log_settings& settings) {
        try {
            // ==============================================
            // 判断：创建 异步日志器 / 同步日志器
            // ==============================================
            if (settings.async) {
                // --------------------------
                // 【异步日志】初始化流程
                // --------------------------

                // 1. 初始化 spdlog 异步线程池
                // 参数1：队列大小 8192
                // 参数2：工作线程数量 1
                spdlog::init_thread_pool(8192, 1);

                // 2. 定义 sink 集合（一个logger可以绑定多个输出目标）
                std::vector<spdlog::sink_ptr> sinks;

                // 3. 根据配置选择输出方式：控制台 或 文件
                if (settings.path == "stdout") {
                    // 创建【多线程安全】的控制台彩色输出 sink
                    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
                } else {
                    // 创建【多线程安全】的旋转文件 sink
                    // 参数：文件路径、单文件最大5MB、最多保留3个文件
                    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                        settings.path, 1024 * 1024 * 5, 3
                    ));
                }

                // 4. 创建异步 logger（正确方式！）
                // 参数：日志器名称、sink迭代器、线程池、溢出策略（阻塞）
                g_logger = std::make_shared<spdlog::async_logger>(
                    "async_logger",
                    sinks.begin(),
                    sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block
                );

            } else {
                // --------------------------
                // 【同步日志】初始化流程
                // --------------------------

                // 根据配置选择输出方式：控制台 或 文件
                if (settings.path == "stdout") {
                    // 创建多线程安全的控制台日志器
                    g_logger = spdlog::stdout_color_mt("stdout_logger");
                } else {
                    // 创建多线程安全的基础文件日志器
                    g_logger = spdlog::basic_logger_mt("file_logger", settings.path);
                }
            }

            // ==============================================
            // 设置日志输出级别
            // ==============================================
            switch (settings.level) {
                case 1: g_logger->set_level(spdlog::level::debug); break;   // 调试级别
                case 2: g_logger->set_level(spdlog::level::info);  break;   // 普通信息
                case 3: g_logger->set_level(spdlog::level::warn);  break;   // 警告
                case 4: g_logger->set_level(spdlog::level::err);   break;   // 错误
                case 6: g_logger->set_level(spdlog::level::off);   break;   // 关闭日志
                default: g_logger->set_level(spdlog::level::info); break;   // 默认 info
            }

            // ==============================================
            // 设置日志输出格式（由外部传入）
            // ==============================================
            g_logger->set_pattern(settings.format);

            // ==============================================
            // 注册为 spdlog 全局默认 logger
            // 之后可以直接用 spdlog::info() 等全局函数
            // ==============================================
            spdlog::set_default_logger(g_logger);

        } catch (const spdlog::spdlog_ex& ex) {
            // 捕获 spdlog 异常并打印错误信息
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }
}