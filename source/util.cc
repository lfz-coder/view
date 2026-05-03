#include "util.h"

namespace view {
    // json 相关工具实现
    // json 对象序列化为字符串
    std::optional<std::string> JsonUtil::serialize(const Json::Value& json) {
        Json::StreamWriterBuilder swb; // 创建 StreamWriterBuilder 对象
        // 个性化设置
        swb["indentation"] = ""; // 设置缩进为空字符串，表示不使用缩进
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter()); // 创建 StreamWriter 对象
        std::ostringstream oss; // 创建输出字符串流
        int ret = sw->write(json, &oss); // 将 json 对象写入字符串流
        if(ret != 0) {
            ERROR("序列化失败!");
            return std::nullopt; // 如果写入失败，返回 std::nullopt
        }
        return oss.str(); // 返回字符串流中的内容
    }
    // json 字符串反序列化为 json 对象
    std::optional<Json::Value> JsonUtil::deserialize(const std::string& input) {
        Json::CharReaderBuilder crb; // 创建 CharReaderBuilder 对象
        std::unique_ptr<Json::CharReader> reader(crb.newCharReader()); // 创建 CharReader 对象
        Json::Value val;
        std::string errs; // 用于存储错误信息
        if (!reader->parse(input.c_str(), input.c_str() + input.length(), &val, &errs)) {
            ERROR("{}反序列化失败: {}", input, errs);
            return std::nullopt; // 如果解析失败，返回 std::nullopt
        }
        return val;
    }
}