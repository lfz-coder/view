/**
 * @file viewUtil.h
 * @brief 通用工具模块 —— JSON / 文件 / 字符串 / 随机数
 * @author Your Name
 * @date 2026
 *
 * 提供以下工具类：
 * - JsonUtil：  JSON 序列化/反序列化（基于 jsoncpp）
 * - FileUtil：  文件读写
 * - StrUtil：   字符串分割
 * - RandomUtil：随机字符串生成
 */

#pragma once
#include <jsoncpp/json/json.h>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>

#include "viewLog.h"

namespace viewUtil {
    // json 相关工具
    class JsonUtil {
    public:
        // 将 json 对象序列化为字符串
        static std::optional<std::string> serialize(const Json::Value& json);

        // 将字符串反序列化为 json 对象
        static std::optional<Json::Value> deserialize(const std::string& str);
    };

    // 文件相关
    class FileUtil {
    public:
        static bool Read(const std::string& path, std::string& content);
        static bool Write(const std::string& path, const std::string& content);
    };

    // 字符串相关处理工具方法
    class StrUtil {
    public:
        static size_t Split(const std::string& str, const std::string& delimiter, std::vector<std::string>& out);
    };

    const size_t RANDOM_STRING_DEFAULT_LENGTH = 16; // 随机字符串默认长度
    enum class RandomCharType {
        kMix,   // 字母 + 数字
        kChar,  // 纯字母
        kDigit  // 纯数字
    };
    class RandomUtil {
    public:
        // 生成指定长度和字符类型的随机字符串
        static std::string RandomString(size_t length = RANDOM_STRING_DEFAULT_LENGTH,
                                         RandomCharType type = RandomCharType::kMix);
    };
}