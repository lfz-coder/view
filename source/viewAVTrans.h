/**
 * @file viewAVTrans.h
 * @brief HLS（HTTP Live Streaming）视频转码器模块
 * @author Your Name
 * @date 2026-05-07
 * 
 * 该模块提供了将各种格式的视频文件转换为 HLS 流媒体格式的功能。
 * HLS 是 Apple 公司提出的 HTTP 流媒体通信协议，它将视频文件切分成多个小的 TS 分片，
 * 并生成一个 M3U8 索引文件，用于实现自适应比特率流式传输。
 * 
 * 主要功能：
 * - 解析已有的 M3U8 文件（M3U8Info 类）
 * - 生成新的 M3U8 文件（M3U8Info 类）
 * - 将输入视频转换为 HLS 格式（HLSTransCoder 类，重新封装模式）
 */

#pragma once

// ==================== FFmpeg 头文件 ====================
/**
 * @extern "C"
 * @brief 告诉 C++ 编译器按照 C 语言的符号命名规则处理以下头文件
 * 
 * FFmpeg 是纯 C 语言编写的库，使用 C 的符号命名规则（无名称修饰）。
 * C++ 编译器默认会对函数名进行修饰以支持重载等特性，这会导致链接时找不到 FFmpeg 的函数。
 * extern "C" 块告诉 C++ 编译器：里面的函数按 C 规则生成符号名。
 */
extern "C" {
#include <libavformat/avformat.h>  ///< 封装格式处理（打开/读取/写入视频容器，HLS 封装）
#include <libavcodec/avcodec.h>    ///< 编解码器相关（编码参数结构体、编解码器上下文）
#include <libavutil/avutil.h>      ///< 通用工具函数（日志、错误码、时间基转换等）
#include <libswscale/swscale.h>    ///< 图像缩放转换（保留以备将来实现真正的转码功能）
#include <libavutil/error.h>       ///< 错误处理相关（将错误码转换为可读字符串）
}

// ==================== C++ 标准库头文件 ====================
#include <iostream>      ///< 标准输入输出流（用于打印日志和错误信息）
#include <vector>        ///< 动态数组（用于存储流映射关系、M3U8 头部和 URL 对）
#include <string>        ///< 字符串类（用于存储文件路径、URL、M3U8 标签等）

/**
 * @namespace viewAVTrans
 * @brief 音视频转换相关功能的命名空间
 * 
 * 该命名空间包含了 HLS 转换相关的所有类和函数，避免与全局命名空间或其他库的符号冲突。
 */
namespace viewAVTrans {

// ==================== M3U8 标签常量 ====================
/**
 * @name M3U8 标签常量
 * @brief M3U8 播放列表文件中使用的标准标签字符串
 * 
 * 根据 HLS 协议规范（RFC 8216），M3U8 文件使用以 '#' 开头的标签来携带元数据。
 * 这些常量用于解析和生成 M3U8 文件。
 * 
 * @see https://datatracker.ietf.org/doc/html/rfc8216
 */
///@{
const std::string HLS_EXTM3U = "#EXTM3U";                       ///< M3U8 文件头标识
const std::string HLS_VERSION = "#EXT-X-VERSION:";              ///< HLS 协议版本号
const std::string HLS_TARGETDURATION = "#EXT-X-TARGETDURATION:"; ///< 目标分片时长（秒）
const std::string HLS_MEDIA_SEQUENCE = "#EXT-X-MEDIA-SEQUENCE:"; ///< 第一个分片的序列号
const std::string HLS_PLAYLIST_TYPE = "#EXT-X-PLAYLIST-TYPE:"; ///< 播放列表类型（VOD/EVENT）
const std::string HLS_INDEPENDENT_SEGMENTS = "#EXT-X-INDEPENDENT-SEGMENTS"; ///< 分片可独立解码标识
const std::string HLS_EXTINF = "#EXTINF:";                      ///< 分片时长信息
const std::string HLS_ENDLIST = "#EXT-X-ENDLIST";               ///< 播放列表结束标记
///@}

/**
 * @class M3U8Info
 * @brief M3U8 播放列表文件的解析和生成类
 * 
 * M3U8 是 HLS 协议中的播放列表文件，它是一个 UTF-8 编码的文本文件，
 * 包含了一系列 TS 分片的 URL 地址和播放元数据（时长、分辨率等）。
 * 
 * 该类提供了解析已有 M3U8 文件和生成新 M3U8 文件的功能。
 * 
 * M3U8 文件示例：
 * @code{.m3u8}
 * #EXTM3U
 * #EXT-X-VERSION:3
 * #EXT-X-TARGETDURATION:10
 * #EXT-X-MEDIA-SEQUENCE:0
 * #EXTINF:10.0,
 * http://example.com/segment0.ts
 * #EXTINF:10.0,
 * http://example.com/segment1.ts
 * #EXT-X-ENDLIST
 * @endcode
 */
class M3U8Info {
public:
    using Ptr = std::shared_ptr<M3U8Info>; ///< M3U8Info 类的智能指针类型别名
    /**
     * @brief URL 键值对类型别名
     * 
     * - first：  分片时长字符串（如 "10.0"）
     * - second： 分片的 URL 地址（如 "http://example.com/segment0.ts"）
     */
    using StrPair = std::pair<std::string, std::string>;

    /**
     * @brief 构造函数
     * @param fileName M3U8 文件的路径（可以是本地路径或 URL）
     * 
     * 只保存文件路径，不进行实际的解析操作。
     * 需要显式调用 Parse() 方法来解析文件内容。
     */
    M3U8Info(const std::string& fileName);

    /**
     * @brief 解析 M3U8 文件
     * @return true 解析成功，false 解析失败
     * 
     * 读取 M3U8 文件，将内容分为两部分：
     * - 头部信息（以 # 开头的标签行，如 #EXTM3U、#EXT-X-VERSION 等）
     * - URL 键值对（分片时长和对应的 URL）
     * 
     * 解析结果存储在 _headers 和 _urlPairs 成员变量中。
     * 
     * @note 遇到 #EXT-X-ENDLIST 标签时会停止解析后续内容。
     */
    bool Parse();

    /**
     * @brief 生成/写入 M3U8 文件
     * @return true 写入成功，false 写入失败
     * 
     * 根据内存中的 _headers 和 _urlPairs 数据，重新生成 M3U8 文件。
     * 文件会覆盖写入到构造函数中指定的路径。
     * 
     * @note 通常在修改了 URL 列表或头部信息后调用此方法保存更改。
     */
    bool Write();

    /**
     * @brief 获取 M3U8 文件的头部信息列表
     * @return std::vector<std::string>& 头部信息行的引用
     * 
     * 头部信息包括但不限于：
     * - #EXTM3U                （文件标识）
     * - #EXT-X-VERSION:3       （协议版本）
     * - #EXT-X-TARGETDURATION:10（目标分片时长）
     * - #EXT-X-MEDIA-SEQUENCE:0（起始序列号）
     * - #EXT-X-ENDLIST         （结束标记，点播模式会有）
     * 
     * @note 返回的是引用，可以直接修改，修改后需要调用 Write() 保存。
     */
    std::vector<std::string>& GetHeaders();

    /**
     * @brief 获取分片 URL 键值对列表
     * @return std::vector<StrPair>& URL 键值对列表的引用
     * 
     * 每个键值对包含：
     * - first：  分片时长（字符串格式，如 "10.0"）
     * - second： 分片的 URL 地址（如 "http://example.com/segment0.ts"）
     * 
     * 遍历这个列表可以获取所有 TS 分片的信息。
     * 
     * @note 返回的是引用，可以直接修改，修改后需要调用 Write() 保存。
     */
    std::vector<StrPair>& GetUrlPairs();

private:
    std::string _fileName;                ///< M3U8 文件的路径
    std::vector<std::string> _headers;    ///< M3U8 头部信息（以 # 开头的标签行）
    std::vector<StrPair> _urlPairs;       ///< 分片 URL 键值对（时长, URL）
};

/**
 * @struct HLSConfig
 * @brief HLS 转换配置参数结构体
 * 
 * 用于配置 HLS 转换的行为，包括分片时长、播放类型、基础 URL 等。
 * 使用者可以根据需要修改这些参数来定制 HLS 输出。
 */
struct HLSConfig {
    /**
     * @brief 每个 TS 分片的目标时长（秒）
     * 
     * FFmpeg 会尽量接近这个值，但实际分片时长可能略有差异，
     * 因为分片必须在关键帧（I 帧）处切割。
     * 
     * 典型值：5-10 秒之间。
     * - 较短的时长：启动更快，但文件数量更多，HTTP 请求更频繁
     * - 较长的时长：启动稍慢，但文件数量少，服务器压力小
     */
    int hls_time = 5;

    /**
     * @brief HLS 播放列表类型
     * 
     * 可选值：
     * - "vod"：  点播模式（Video On Demand）
     *            所有分片在 M3U8 中完整列出，适合已完整录制好的视频
     *            包含 #EXT-X-ENDLIST 标记
     * - "event"：事件模式
     *            分片边生成边列出，适合直播结束后保留完整录像
     *            不包含 #EXT-X-ENDLIST，播放器会持续请求新分片
     * - "":      直播模式（空字符串）
     *            动态更新列表，只保留最近几个分片
     * 
     * @note 对于离线转码需求，通常使用 "vod" 模式。
     */
    std::string hls_playlist_type = "vod";

    /**
     * @brief TS 分片文件的 URL 基础前缀
     * 
     * 播放器会将此前缀与分片文件名拼接成完整的 URL 来下载 TS 分片。
     * 
     * 示例：
     * @code
     * hls_base_url = "http://192.168.1.100:9000/video/"
     * 分片文件名 = "hls0.ts"
     * 完整 URL = "http://192.168.1.100:9000/video/hls0.ts"
     * @endcode
     * 
     * @note 如果为空字符串，则 M3U8 中使用相对路径。
     */
    std::string hls_base_url; ///< TS 分片文件的 URL 基础前缀（默认为空，使用相对路径）
};

/**
 * @class HLSTransCoder
 * @brief HLS 视频转码器类
 * 
 * 该类封装了使用 FFmpeg 将输入视频文件转换为 HLS 格式的核心逻辑。
 * 
 * 工作流程：
 * 1. 打开输入视频文件并解析其流信息
 * 2. 创建 HLS 输出上下文，配置分片参数
 * 3. 复制输入流的编码参数到输出流（重新封装，不重新编码）
 * 4. 逐帧读取输入视频，转换时间戳后写入 HLS 输出
 * 5. 生成 .m3u8 播放列表文件和 .ts 分片文件
 * 
 * @note 当前实现采用 remux（重新封装）模式，不进行真正的编解码，
 *       因此转换速度很快且无损质量，但要求输入格式必须被 HLS 容器支持。
 *       如需改变编码参数（如降低码率、分辨率），需要添加编码器逻辑。
 * 
 * @see HLSConfig
 */
class HLSTransCoder {
    /**
     * @struct NetworkGuard
     * @brief RAII 模式的网络库守护类
     * 
     * 在构造时调用 avformat_network_init() 初始化网络库，
     * 在析构时调用 avformat_network_deinit() 清理网络库。
     * 
     * 使用 RAII 模式确保网络库的初始化和清理总是成对执行，
     * 即使在发生异常或提前返回的情况下也能正确清理资源。
     */
    struct NetworkGuard {
        /**
         * @brief 构造函数，初始化 FFmpeg 网络库
         * 
         * 在 Windows 平台上会调用 WSAStartup() 初始化 Winsock，
         * 在其他平台基本无操作或初始化 SSL/TLS 库。
         */
        NetworkGuard() { avformat_network_init(); }
        
        /**
         * @brief 析构函数，清理 FFmpeg 网络库
         * 
         * 在 Windows 平台上会调用 WSACleanup() 清理 Winsock，
         * 在其他平台基本无操作或清理 SSL/TLS 库。
         */
        ~NetworkGuard() { avformat_network_deinit(); }
    };

public:
    using Ptr = std::shared_ptr<HLSTransCoder>; ///< HLSTransCoder 类的智能指针类型别名
    /**
     * @brief 构造函数
     * @param config HLS 转换配置参数
     * 
     * 使用指定的配置参数创建一个 HLS 转换器实例。
     * 配置参数会保存在成员变量中，供后续调用 Transcode 时使用。
     */
    HLSTransCoder(const HLSConfig& config);

    /**
     * @brief 执行 HLS 转码
     * @param inputFile  输入视频文件路径（支持 FFmpeg 支持的所有格式）
     * @param outputFile 输出 M3U8 文件路径（如 "output/playlist.m3u8"）
     * @return true  转码成功
     * @return false 转码失败（错误信息会通过 viewLog 输出）
     * 
     * 将输入视频文件转换为 HLS 格式，生成 .m3u8 和 .ts 文件。
     * TS 分片文件会自动生成在与 M3U8 文件相同的目录下，
     * 文件名为 M3U8 文件名去掉 .m3u8 后缀后加上序号。
     * 
     * 示例：
     * @code
     * HLSConfig config;
     * config.hls_time = 10;
     * config.hls_playlist_type = "vod";
     * 
     * HLSTransCoder coder(config);
     * coder.Transcode("input.mp4", "/var/www/hls/playlist.m3u8");
     * // 生成文件：
     * //   /var/www/hls/playlist.m3u8
     * //   /var/www/hls/playlist0.ts
     * //   /var/www/hls/playlist1.ts
     * //   ...
     * @endcode
     * 
     * @note 输出目录必须存在，程序不会自动创建目录。
     * @note 该函数会阻塞直到转换完成，适合离线转码场景。
     * @note 转换过程中会通过 viewLog 输出进度信息和错误详情。
     */
    bool Transcode(const std::string& inputFile, const std::string& outputFile);

protected:
    /**
     * @brief 将 FFmpeg 错误码转换为可读的字符串描述
     * @param errnum FFmpeg 函数返回的错误码（负数）
     * @return const char* 错误描述的字符串指针
     * 
     * 该函数使用 thread_local 静态缓冲区，每个线程都有独立的缓冲区，
     * 因此可以在多线程环境中安全使用。返回的指针指向线程局部的缓冲区，
     * 不需要手动释放内存。
     * 
     * 使用示例：
     * @code
     * int ret = avformat_open_input(...);
     * if (ret < 0) {
     *     viewLog::ERROR("打开文件失败: {}", AvError(ret));
     * }
     * @endcode
     * 
     * @note 返回的指针在同一个线程的下一次调用此函数时可能被覆盖，
     *       因此应该在获取错误信息后立即使用或复制。
     */
    static const char *AvError(int errnum);

private:
    HLSConfig _config;   ///< HLS 转换配置参数
};

} // namespace viewAVTrans