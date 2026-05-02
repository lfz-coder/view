#include <viewLog.h>
#include <gflags/gflags.h>

DEFINE_bool(async, false, "Use async logging");
DEFINE_int32(level, 2, "Log level");
DEFINE_string(format, "[%H:%M:%S][%-7l]%v", "Log format");
DEFINE_string(path, "stdout", "Log file path");

int main() {
    // 初始化日志配置
    view::log_settings settings;
    settings.async = FLAGS_async; // 同步日志
    settings.level = FLAGS_level; // 日志级别
    settings.format = FLAGS_format; // 日志格式
    settings.path = FLAGS_path; // 日志文件路径
    // 初始化日志器
    view::init_logger(settings);
    // 输出日志
    view::DEBUG("This is a debug message with value: {}", 42);
    view::INFO("This is an info message");
    view::WARN("This is a warning message");
    view::ERROR("This is an error message");
    return 0;
}