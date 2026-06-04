/**
 * @file viewUtil.cc
 * @brief 通用工具模块实现 —— JsonUtil / FileUtil / StrUtil / RandomUtil
 */

#include "viewUtil.h"
#include "viewLog.h"

#include <random>
#include <chrono>

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

    // *********************************************************************************************** //

    std::string RandomUtil::RandomString(size_t length /* = RANDOM_STRING_DEFAULT_LENGTH */,
                                          RandomCharType type /* = RandomCharType::kMix */) {
        // ===================== 步骤1：根据类型选择字符集 =====================
        // 定义3种静态只读字符集（static 全局唯一，const 不可修改，线程安全）
        static const char* charSetMix = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static const char* charSetChar = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static const char* charSetDigit = "0123456789";

        // 指向最终选中的字符集
        const char* charSet = nullptr;
        switch (type) {
            case RandomCharType::kMix:  charSet = charSetMix;  break;
            case RandomCharType::kChar: charSet = charSetChar; break;
            case RandomCharType::kDigit: charSet = charSetDigit; break;
            default: charSet = charSetMix; break;
        }

        // 获取字符集总长度
        size_t charSetSize = strlen(charSet);
        // 预分配字符串内存，避免多次扩容，提升性能
        std::string result;
        result.reserve(length);

        // ===================== 线程安全随机数生成器 =====================
        // thread_local: 每个线程独立一份，无竞争，绝对安全
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        // 生成 [0, 字符集长度-1] 范围的随机数
        std::uniform_int_distribution<> dist(0, static_cast<int>(charSetSize) - 1);

        // ===================== 核心：处理长度 <4 的情况 =====================
        // 固定后缀长度：只有总长度 >=4 时，才使用4位后缀，否则不使用后缀
        size_t fixedSuffixLength = (length >= 4) ? 4 : 0;
        // 随机部分长度 = 总长度 - 后缀长度
        size_t randomPartLength = length - fixedSuffixLength;

        // ===================== 步骤2：生成随机字符部分 =====================
        for (size_t i = 0; i < randomPartLength; ++i) {
            // 从字符集中随机取一个字符
            result += charSet[dist(gen)];
        }

        // ===================== 步骤3：长度>=4时，追加4位固定编号 =====================
        if (fixedSuffixLength > 0) {
            // 用时间戳生成4位唯一后缀
            uint32_t timestamp = static_cast<uint32_t>(time(nullptr));
            for (int i = 0; i < 4; ++i) {
                result += charSet[timestamp % charSetSize];
                // 右移，保证每一位都不同
                timestamp >>= 6;
            }
        }

        // ===================== 步骤4：返回最终拼接好的字符串 =====================
        return result;
    }


}