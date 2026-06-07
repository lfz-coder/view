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

    /**
     * @class JsonUtil
     * @brief JSON 序列化/反序列化工具类（基于 jsoncpp）
     *
     * 提供 JSON 对象与字符串之间的双向转换，所有方法均为静态方法。
     * 失败时返回 std::nullopt 并通过 viewLog 输出错误信息。
     */
    class JsonUtil {
    public:
        /**
         * @brief 将 JSON 对象序列化为字符串
         * @param json 待序列化的 JSON 对象
         * @return 成功返回 JSON 字符串，失败返回 std::nullopt
         */
        static std::optional<std::string> serialize(const Json::Value& json);

        /**
         * @brief 将 JSON 字符串反序列化为 JSON 对象
         * @param str 待解析的 JSON 字符串
         * @return 成功返回 JSON 对象，失败返回 std::nullopt
         */
        static std::optional<Json::Value> deserialize(const std::string& str);
    };

    /**
     * @class FileUtil
     * @brief 文件读写工具类
     *
     * 以二进制模式进行文件的读/写操作，支持完整的错误处理。
     * 所有方法均为静态方法。
     */
    class FileUtil {
    public:
        /**
         * @brief 以二进制模式读取文件全部内容
         * @param path    文件路径
         * @param content [出参] 读取到的文件内容（指针，不可为空）
         * @return true 读取成功，false 读取失败
         */
        static bool Read(const std::string& path, std::string* content);

        /**
         * @brief 以二进制模式将内容写入文件（覆盖写入）
         * @param path    文件路径
         * @param content 待写入的内容
         * @return true 写入成功，false 写入失败
         */
        static bool Write(const std::string& path, const std::string& content);
    };

    /**
     * @class StrUtil
     * @brief 字符串处理工具类
     *
     * 提供常用的字符串操作，所有方法均为静态方法。
     */
    class StrUtil {
    public:
        /**
         * @brief 按分隔符拆分字符串
         * @param str       待拆分的原始字符串
         * @param delimiter 分隔符字符串
         * @param out       [出参] 拆分后的子串列表（指针，不可为空）
         * @return 拆分出的子串数量
         */
        static size_t Split(const std::string& str, const std::string& delimiter, std::vector<std::string>* out);
    };

    /// 随机字符串默认生成长度
    const size_t RANDOM_STRING_DEFAULT_LENGTH = 16;

    /**
     * @enum RandomCharType
     * @brief 随机字符串的字符类型枚举
     */
    enum class RandomCharType {
        kMix,   ///< 字母 + 数字混合
        kChar,  ///< 纯字母
        kDigit  ///< 纯数字
    };

    /**
     * @class RandomUtil
     * @brief 随机字符串生成工具类
     *
     * 使用线程安全的随机数生成器，支持指定长度和字符类型。
     * 长度 >= 4 时会追加 4 位基于时间戳的后缀以保证唯一性。
     */
    class RandomUtil {
    public:
        /**
         * @brief 生成指定长度和字符类型的随机字符串
         * @param length 目标字符串长度，默认 16
         * @param type   字符类型，默认 kMix（字母+数字混合）
         * @return 生成的随机字符串
         *
         * @note 当 length >= 4 时，末尾 4 位为基于时间戳生成的固定编号，
         *       剩余前缀为完全随机字符，用于降低碰撞概率。
         */
        static std::string RandomString(size_t length = RANDOM_STRING_DEFAULT_LENGTH,
                                         RandomCharType type = RandomCharType::kMix);
    };

}