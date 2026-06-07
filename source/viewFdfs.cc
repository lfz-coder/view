/**
 * @file viewFdfs.cc
 * @brief FastDFS 分布式文件系统客户端模块实现
 * 
 * 实现基于 FastDFS C API 的文件上传、下载、删除功能。
 */

#include "viewFdfs.h"
#include "viewLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

extern "C" {
#include <fastcommon/connection_pool.h>
}

namespace viewFdfs {

// ==================== 文件 ID 缓冲区大小常量 ====================
static const int FILE_ID_BUFFER_SIZE = 256; ///< 文件 ID 缓冲区大小（FDFS_GROUP_NAME_MAX_LEN=16 + 足够存储文件名）

// ==================== FdfsClient::Init 实现 ====================

/**
 * @brief 初始化 FastDFS 客户端
 *
 * 根据 FdfsSettings 配置参数生成 FastDFS 格式的配置字符串，
 * 然后调用 fdfs_client_init_from_buffer() 完成初始化。
 *
 * 生成的配置格式：
 * @code{.ini}
 * connect_timeout=30
 * network_timeout=30
 * tracker_server=192.168.1.100:22122
 * use_connection_pool=true
 * connection_pool_max_idle_time=3600
 * @endcode
 */
bool FdfsClient::Init(const FdfsSettings& settings) {
    // 初始化 fastcommon 日志器，否则后续 logError 调用会因 log_buff 为 NULL 而崩溃
    log_init();

    if (settings.trackerServers.empty()) {
        viewLog::ERROR("FdfsClient::Init 失败: trackerServers 为空，至少需要一个 Tracker 服务器地址");
        return false;
    }

    // 步骤1：构建 FastDFS 客户端配置字符串
    std::ostringstream configStream;
    configStream << "connect_timeout=" << settings.connectTimeout << "\n";
    configStream << "network_timeout=" << settings.networkTimeout << "\n";
    configStream << "use_connection_pool=" << (settings.useConnectionPool ? "true" : "false") << "\n";
    configStream << "connection_pool_max_idle_time=" << settings.connectionPoolMaxIdleTime << "\n";

    for (const auto& server : settings.trackerServers) {
        configStream << "tracker_server=" << server << "\n";
    }

    std::string configStr = configStream.str();
    viewLog::INFO("FdfsClient::Init 配置: {}", configStr);

    // 步骤2：从配置字符串初始化 FastDFS 客户端
    int result = fdfs_client_init_from_buffer(configStr.c_str());
    if (result != 0) {
        viewLog::ERROR("FdfsClient::Init 失败: fdfs_client_init_from_buffer 返回错误码 {}", result);
        return false;
    }

    viewLog::INFO("FdfsClient::Init 成功，已连接 {} 个 Tracker 服务器", settings.trackerServers.size());
    return true;
}

// ==================== FdfsClient::Destroy 实现 ====================

/**
 * @brief 销毁 FastDFS 客户端，释放所有资源
 */
void FdfsClient::Destroy() {
    fdfs_client_destroy();
    log_destroy();
    viewLog::INFO("FdfsClient::Destroy 已释放所有资源");
}

// ==================== FdfsClient::UploadFromBuff 实现 ====================

/**
 * @brief 从内存缓冲区上传文件到 FastDFS
 *
 * 上传流程：
 * 1. 获取 Tracker 连接
 * 2. 询问 Tracker 可用的 Storage 服务器
 * 3. 将缓冲区数据上传到 Storage 服务器
 * 4. 断开 Storage 连接，返回文件 ID
 */
std::optional<std::string> FdfsClient::UploadFromBuff(const std::string& buff) {
    if (buff.empty()) {
        viewLog::ERROR("FdfsClient::UploadFromBuff 失败: 缓冲区为空");
        return std::nullopt;
    }

    // 步骤1：获取 Tracker 连接
    ConnectionInfo* pTrackerServer = tracker_get_connection();
    if (pTrackerServer == nullptr) {
        viewLog::ERROR("FdfsClient::UploadFromBuff 失败: 无法获取 Tracker 连接");
        return std::nullopt;
    }

    // 步骤2：查询可用的 Storage 服务器
    ConnectionInfo storageServer;
    char groupName[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
    int storePathIndex = 0;
    int result = tracker_query_storage_store(pTrackerServer, &storageServer,
                                              groupName, &storePathIndex);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::UploadFromBuff 失败: 查询 Storage 服务器失败，错误码 {}", result);
        return std::nullopt;
    }

    // 步骤3：上传文件到 Storage 服务器
    char fileId[FILE_ID_BUFFER_SIZE] = {0};
    result = storage_upload_by_filebuff1(pTrackerServer, &storageServer,
                                          storePathIndex,
                                          buff.data(), static_cast<int64_t>(buff.size()),
                                          nullptr,   // file_ext_name: 无扩展名
                                          nullptr, 0, // meta_list, meta_count: 无元数据
                                          groupName,
                                          fileId);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::UploadFromBuff 失败: 上传失败，错误码 {}", result);
        conn_pool_disconnect_server(&storageServer);
        return std::nullopt;
    }

    // 步骤4：断开 Storage 连接，返回文件 ID
    conn_pool_disconnect_server(&storageServer);
    std::string fileIdStr(fileId);
    viewLog::INFO("FdfsClient::UploadFromBuff 成功: fileId={}", fileIdStr);
    return fileIdStr;
}

// ==================== FdfsClient::UploadFromFile 实现 ====================

/**
 * @brief 从本地文件上传到 FastDFS
 *
 * 上传流程：
 * 1. 获取 Tracker 连接
 * 2. 询问 Tracker 可用的 Storage 服务器
 * 3. 将本地文件上传到 Storage 服务器
 * 4. 断开 Storage 连接，返回文件 ID
 */
std::optional<std::string> FdfsClient::UploadFromFile(const std::string& filePath) {
    if (filePath.empty()) {
        viewLog::ERROR("FdfsClient::UploadFromFile 失败: 文件路径为空");
        return std::nullopt;
    }

    // 步骤1：获取 Tracker 连接
    ConnectionInfo* pTrackerServer = tracker_get_connection();
    if (pTrackerServer == nullptr) {
        viewLog::ERROR("FdfsClient::UploadFromFile 失败: 无法获取 Tracker 连接");
        return std::nullopt;
    }

    // 步骤2：查询可用的 Storage 服务器
    ConnectionInfo storageServer;
    char groupName[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
    int storePathIndex = 0;
    int result = tracker_query_storage_store(pTrackerServer, &storageServer,
                                              groupName, &storePathIndex);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::UploadFromFile 失败: 查询 Storage 服务器失败，错误码 {}", result);
        return std::nullopt;
    }

    // 步骤3：上传本地文件到 Storage 服务器
    char fileId[FILE_ID_BUFFER_SIZE] = {0};
    result = storage_upload_by_filename1(pTrackerServer, &storageServer,
                                          storePathIndex,
                                          filePath.c_str(),
                                          nullptr,   // file_ext_name: NULL 表示自动从文件名提取
                                          nullptr, 0, // meta_list, meta_count: 无元数据
                                          groupName,
                                          fileId);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::UploadFromFile 失败: 上传文件 {} 失败，错误码 {}",
                       filePath, result);
        conn_pool_disconnect_server(&storageServer);
        return std::nullopt;
    }

    // 步骤4：断开 Storage 连接，返回文件 ID
    conn_pool_disconnect_server(&storageServer);
    std::string fileIdStr(fileId);
    viewLog::INFO("FdfsClient::UploadFromFile 成功: {} -> fileId={}", filePath, fileIdStr);
    return fileIdStr;
}

// ==================== FdfsClient::DownloadToBuff 实现 ====================

/**
 * @brief 下载 FastDFS 文件到内存缓冲区
 *
 * 下载流程：
 * 1. 获取 Tracker 连接
 * 2. 根据文件 ID 查询所在的 Storage 服务器
 * 3. 从 Storage 服务器下载文件内容到内存
 * 4. 将下载的数据写入输出缓冲区，释放 C 分配的内存
 *
 * @note download API 内部会 malloc 分配缓冲区，需要手动 free。
 */
bool FdfsClient::DownloadToBuff(const std::string& fileId, std::string* outBuff) {
    if (fileId.empty()) {
        viewLog::ERROR("FdfsClient::DownloadToBuff 失败: fileId 为空");
        return false;
    }
    if (outBuff == nullptr) {
        viewLog::ERROR("FdfsClient::DownloadToBuff 失败: outBuff 为 nullptr（输出型参数必须为非空指针）");
        return false;
    }

    // 步骤1：获取 Tracker 连接
    ConnectionInfo* pTrackerServer = tracker_get_connection();
    if (pTrackerServer == nullptr) {
        viewLog::ERROR("FdfsClient::DownloadToBuff 失败: 无法获取 Tracker 连接");
        return false;
    }

    // 步骤2：根据 fileId 查询所在的 Storage 服务器
    ConnectionInfo storageServer;
    int result = tracker_query_storage_fetch1(pTrackerServer, &storageServer,
                                               fileId.c_str());
    if (result != 0) {
        viewLog::ERROR("FdfsClient::DownloadToBuff 失败: 查询文件 {} 所在 Storage 失败，错误码 {}",
                       fileId, result);
        return false;
    }

    // 步骤3：下载文件内容到内存
    char* fileBuff = nullptr;
    int64_t fileSize = 0;
    result = storage_download_file_to_buff1(pTrackerServer, &storageServer,
                                             fileId.c_str(),
                                             &fileBuff, &fileSize);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::DownloadToBuff 失败: 下载文件 {} 失败，错误码 {}",
                       fileId, result);
        conn_pool_disconnect_server(&storageServer);
        return false;
    }

    // 步骤4：将下载的数据写入输出缓冲区，释放 C 分配的内存
    if (fileBuff != nullptr && fileSize > 0) {
        outBuff->assign(fileBuff, static_cast<size_t>(fileSize));
        free(fileBuff);
    } else {
        outBuff->clear();
    }

    conn_pool_disconnect_server(&storageServer);
    viewLog::INFO("FdfsClient::DownloadToBuff 成功: fileId={} size={}", fileId, fileSize);
    return true;
}

// ==================== FdfsClient::DownloadToFile 实现 ====================

/**
 * @brief 下载 FastDFS 文件到本地文件系统
 *
 * 下载流程：
 * 1. 获取 Tracker 连接
 * 2. 根据文件 ID 查询所在的 Storage 服务器
 * 3. 从 Storage 服务器下载文件并写入本地路径
 */
bool FdfsClient::DownloadToFile(const std::string& fileId, const std::string& localFilePath) {
    if (fileId.empty()) {
        viewLog::ERROR("FdfsClient::DownloadToFile 失败: fileId 为空");
        return false;
    }
    if (localFilePath.empty()) {
        viewLog::ERROR("FdfsClient::DownloadToFile 失败: localFilePath 为空");
        return false;
    }

    // 步骤1：获取 Tracker 连接
    ConnectionInfo* pTrackerServer = tracker_get_connection();
    if (pTrackerServer == nullptr) {
        viewLog::ERROR("FdfsClient::DownloadToFile 失败: 无法获取 Tracker 连接");
        return false;
    }

    // 步骤2：根据 fileId 查询所在的 Storage 服务器
    ConnectionInfo storageServer;
    int result = tracker_query_storage_fetch1(pTrackerServer, &storageServer,
                                               fileId.c_str());
    if (result != 0) {
        viewLog::ERROR("FdfsClient::DownloadToFile 失败: 查询文件 {} 所在 Storage 失败，错误码 {}",
                       fileId, result);
        return false;
    }

    // 步骤3：下载文件到本地路径
    int64_t fileSize = 0;
    result = storage_download_file_to_file1(pTrackerServer, &storageServer,
                                             fileId.c_str(),
                                             localFilePath.c_str(),
                                             &fileSize);
    if (result != 0) {
        viewLog::ERROR("FdfsClient::DownloadToFile 失败: 下载文件 {} 到 {} 失败，错误码 {}",
                       fileId, localFilePath, result);
        conn_pool_disconnect_server(&storageServer);
        return false;
    }

    conn_pool_disconnect_server(&storageServer);
    viewLog::INFO("FdfsClient::DownloadToFile 成功: fileId={} -> {} size={}",
                  fileId, localFilePath, fileSize);
    return true;
}

// ==================== FdfsClient::Remove 实现 ====================

/**
 * @brief 删除 FastDFS 上的文件
 *
 * 删除流程：
 * 1. 获取 Tracker 连接
 * 2. 根据文件 ID 查询所在的 Storage 服务器
 * 3. 在 Storage 服务器上删除该文件
 */
bool FdfsClient::Remove(const std::string& fileId) {
    if (fileId.empty()) {
        viewLog::ERROR("FdfsClient::Remove 失败: fileId 为空");
        return false;
    }

    // 步骤1：获取 Tracker 连接
    ConnectionInfo* pTrackerServer = tracker_get_connection();
    if (pTrackerServer == nullptr) {
        viewLog::ERROR("FdfsClient::Remove 失败: 无法获取 Tracker 连接");
        return false;
    }

    // 步骤2：根据 fileId 查询所在的 Storage 服务器（update 用于后续的修改/删除操作）
    ConnectionInfo storageServer;
    int result = tracker_query_storage_update1(pTrackerServer, &storageServer,
                                                fileId.c_str());
    if (result != 0) {
        viewLog::ERROR("FdfsClient::Remove 失败: 查询文件 {} 所在 Storage 失败，错误码 {}",
                       fileId, result);
        return false;
    }

    // 步骤3：删除文件
    result = storage_delete_file1(pTrackerServer, &storageServer, fileId.c_str());
    if (result != 0) {
        viewLog::ERROR("FdfsClient::Remove 失败: 删除文件 {} 失败，错误码 {}", fileId, result);
        conn_pool_disconnect_server(&storageServer);
        return false;
    }

    conn_pool_disconnect_server(&storageServer);
    viewLog::INFO("FdfsClient::Remove 成功: fileId={}", fileId);
    return true;
}

} // namespace viewFdfs
