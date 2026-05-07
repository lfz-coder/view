#include <viewUtil.h>
#include <iostream>

std::optional<std::string> serializeTest() {
    Json::Value json;
    json["name"] = "lfz-coder";
    json["age"] = 30;
    json["score"].append(90.9);
    json["score"].append(83.1);
    json["score"].append(95.2);
    auto strOpt = viewUtil::JsonUtil::serialize(json);
    if (strOpt) {
        std::cout << "序列化结果: \n" << strOpt.value() << std::endl;
        return strOpt.value();
    } else {
        std::cerr << "序列化失败!" << std::endl;
        return std::nullopt;
    }
}

void deserializeTest(const std::string& str) {
    auto jsonOpt = viewUtil::JsonUtil::deserialize(str);
    if (jsonOpt) {
        std::cout << "反序列化结果: \n" << jsonOpt.value() << std::endl;
        std::cout << "姓名: " << jsonOpt.value()["name"] << std::endl;
        std::cout << "年龄: " << jsonOpt.value()["age"] << std::endl;
        std::cout << "成绩: ";
        for (const auto& score : jsonOpt.value()["score"]) {
            std::cout << score.asFloat() << " ";
        }
        std::cout << std::endl;
    } else {
        std::cerr << "反序列化失败!" << std::endl;
    }
}

int main() {
    deserializeTest(serializeTest().value());
    return 0;
}