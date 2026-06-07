/**
 * @file viewFdfs.h
 * @brief FastDFS 分布式文件系统客户端模块
 * @author Your Name
 * @date 2026
 *
 * 该模块封装了 FastDFS C 客户端库，提供文件上传、下载、删除等操作。
 * 支持从内存缓冲区或本地文件上传，支持下载到内存或本地文件。
 *
 * 参数传递规范：
 * - 输入型参数：使用常量引用（const T&）
 * - 输出型参数：使用指针（T*）
 */

#pragma once
#include <iostream>
#include <vector>
#include <optional>
// FastDFS C 头文件会 #define byte signed char，与 C++17 的 std::byte 冲突
// 因此在包含前保存 byte 宏，包含后恢复
#pragma push_macro("byte")
#undef byte
extern "C" {
#include <fastdfs/fdfs_client.h>
#include <fastcommon/logger.h>
}
#pragma pop_macro("byte")

namespace viewFdfs {

/**
 * @struct FdfsSettings
 * @brief FastDFS 客户端配置参数结构体
 *
 * 用于配置 FastDFS 客户端的连接参数，包括 Tracker 服务器地址、
 * 超时设置和连接池参数等。
 */
struct FdfsSettings {
    std::vector<std::string> trackerServers; ///< Tracker 服务器地址列表（格式：IP:Port，如 192.168.1.100:22122）
    int connectTimeout = 30;                 ///< 连接超时时间（秒）
    int networkTimeout = 30;                 ///< 网络读写超时时间（秒）
    bool useConnectionPool = true;           ///< 是否使用连接池（复用连接以提高性能）
    int connectionPoolMaxIdleTime = 3600;    ///< 连接池中连接的最大空闲时间（秒），超时后自动关闭
};

/**
 * @class FdfsClient
 * @brief FastDFS 客户端类
 *
 * 封装了 FastDFS 的文件上传、下载、删除等核心操作。
 * 所有方法均为静态方法，使用前必须先调用 Init() 初始化，
 * 使用完毕后调用 Destroy() 释放资源。
 *
 * 使用示例：
 * @code
 * viewFdfs::FdfsSettings settings;
 * settings.trackerServers.push_back("192.168.1.100:22122");
 * viewFdfs::FdfsClient::Init(settings);
 *
 * // 上传文件
 * auto fileId = viewFdfs::FdfsClient::UploadFromFile("/path/to/file.jpg");
 * if (fileId.has_value()) {
 *     viewLog::INFO("上传成功: {}", fileId.value());
 * }
 *
 * // 下载到内存
 * std::string content;
 * viewFdfs::FdfsClient::DownloadToBuff(fileId.value(), &content);
 *
 * // 删除文件
 * viewFdfs::FdfsClient::Remove(fileId.value());
 *
 * viewFdfs::FdfsClient::Destroy();
 * @endcode
 */
class FdfsClient {
public:
    /**
     * @brief 初始化 FastDFS 客户端
     * @param settings FastDFS 客户端配置参数（输入型参数，使用常量引用）
     * @return true 初始化成功，false 初始化失败
     *
     * 根据配置参数生成 FastDFS 客户端配置，并初始化全局 Tracker 连接。
     * 多次调用此方法会重新初始化，之前的连接会被销毁。
     *
     * @note 调用其他方法之前必须先调用此方法。
     */
    static bool Init(const FdfsSettings& settings);

    /**
     * @brief 销毁 FastDFS 客户端，释放所有资源
     *
     * 关闭所有 Tracker 连接，释放连接池等资源。
     * 销毁后如需再次使用，需要重新调用 Init()。
     */
    static void Destroy();

    /**
     * @brief 从内存缓冲区上传文件
     * @param buff 文件内容缓冲区（输入型参数，使用常量引用）
     * @return 上传成功返回文件 ID（格式："group_name/filename"），失败返回 std::nullopt
     *
     * 将内存中的二进制数据直接上传到 FastDFS 存储服务器。
     * 适用于已经读取到内存中的数据，无需先写入临时文件。
     *
     * @note 上传的文件没有扩展名，如需扩展名请使用 UploadFromFile()。
     */
    static std::optional<std::string> UploadFromBuff(const std::string& buff);

    /**
     * @brief 从本地文件上传
     * @param filePath 本地文件路径（输入型参数，使用常量引用）
     * @return 上传成功返回文件 ID（格式："group_name/filename"），失败返回 std::nullopt
     *
     * 将本地文件上传到 FastDFS 存储服务器。
     * 文件扩展名会自动从文件路径中提取。
     */
    static std::optional<std::string> UploadFromFile(const std::string& filePath);

    /**
     * @brief 下载文件到内存缓冲区
     * @param fileId  FastDFS 文件 ID（输入型参数，使用常量引用）
     * @param outBuff 输出缓冲区指针，下载成功后存储文件内容（输出型参数，使用指针）
     * @return true 下载成功，false 下载失败
     *
     * 将 FastDFS 上的文件内容下载到内存。
     * outBuff 指向的字符串会被覆盖为下载到的文件内容。
     *
     * @note 下载大文件时注意内存占用，大文件建议使用 DownloadToFile()。
     */
    static bool DownloadToBuff(const std::string& fileId, std::string* outBuff);

    /**
     * @brief 下载文件到本地
     * @param fileId        FastDFS 文件 ID（输入型参数，使用常量引用）
     * @param localFilePath 本地保存路径（输入型参数，使用常量引用，指定输出目标路径）
     * @return true 下载成功，false 下载失败
     *
     * 将 FastDFS 上的文件直接保存到本地文件系统。
     * 适用于大文件下载，避免将整个文件加载到内存中。
     *
     * @note 目标目录必须存在，不会自动创建父目录。
     */
    static bool DownloadToFile(const std::string& fileId, const std::string& localFilePath);

    /**
     * @brief 删除文件
     * @param fileId FastDFS 文件 ID（输入型参数，使用常量引用）
     * @return true 删除成功，false 删除失败
     *
     * 从 FastDFS 存储服务器上删除指定文件。
     * 删除操作不可逆，请谨慎使用。
     */
    static bool Remove(const std::string& fileId);
};

} // namespace viewFdfs
