#include "viewUtil.h"
#include "viewLog.h"

namespace viewUtil {
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
            viewLog::ERROR("序列化失败!");
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
            viewLog::ERROR("{}反序列化失败: {}", input, errs);
            return std::nullopt; // 如果解析失败，返回 std::nullopt
        }
        return val;
    }

    // *********************************************************************************************** //

    bool FileUtil::Read(const std::string& path, std::string& content) {
        std::ifstream ifs; // 创建输入文件流对象
        ifs.open(path, std::ios::in | std::ios::binary);
        if(!ifs.is_open()) {
            viewLog::ERROR("FileUtil:Read open failed: {}", path);
            return false; // 如果文件打开失败，返回 false
        }
        ifs.seekg(0, std::ios::end); // 将文件指针移动到文件末尾
        size_t fileSize = ifs.tellg(); // 获取文件大小
        if(fileSize == 0) {
            viewLog::WARN("FileUtil:Read fileSize is 0: {}", path);
            content.clear(); // 如果文件为空，清空内容并返回 true
            return true;
        }
        ifs.seekg(0, std::ios::beg); // 将文件指针移动回文件开头
        content.resize(fileSize); // 调整字符串大小以容纳文件内容
        ifs.read(&content[0], fileSize); // 从文件中读取内容到字符串
        ifs.close(); // 关闭文件流

        return true;
    }

    bool FileUtil::Write(const std::string& path, const std::string& content) {
        std::ofstream ofs; // 创建输出文件流对象
        ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc); // 以二进制模式打开文件，覆盖原有内容
        if(!ofs.is_open()) {
            viewLog::ERROR("FileUtil:Write open failed: {}", path);
            return false; // 如果文件打开失败，返回 false
        }
        ofs.write(content.c_str(), content.length()); // 将内容写入文件
        ofs.close(); // 关闭文件流

        return true;
    }


    // *********************************************************************************************** //

    size_t StrUtil::Split(const std::string& str, const std::string& delimiter, std::vector<std::string>& out) {
        out.clear();
        size_t start = 0;
        size_t end = 0;
        while ((end = str.find(delimiter, start)) != std::string::npos) {
            out.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
        }
        out.push_back(str.substr(start));
        return out.size();
    }

}