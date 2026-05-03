/*
    util 工具封装
        - json 相关工具[序列化和反序列化]
        - 其他工具
*/

#pragma once
#include <jsoncpp/json/json.h>
#include <string>
#include <memory>
#include <sstream>
#include <optional>

#include "viewLog.h"

namespace view {
    // json 相关工具
    class JsonUtil {
    public:
        // 将 json 对象序列化为字符串
        static std::optional<std::string> serialize(const Json::Value& json);

        // 将字符串反序列化为 json 对象
        static std::optional<Json::Value> deserialize(const std::string& str);
    };
}