#include <viewUtil.h>
#include <viewLog.h>
#include <viewFdfs.h>

std::string fileTest() {
    // 基于文件的上传下载测试
    // 1. 将文件上传到 fastdfs 服务器
    auto id = viewFdfs::FdfsClient::UploadFromFile("./makefile");
    if(!id) {
        viewLog::ERROR("上传文件失败");
        abort();
    }
    // 2. 从服务器下载文件到本地
    bool ret = viewFdfs::FdfsClient::DownloadToFile(*id, "./makefile.download1");
    if(!ret) {
        viewLog::ERROR("下载文件失败");
        abort();
    }
    // 3. 通过 MD5sum 命令比较文件是否一致 -- 由我们外部执行命令完成
    viewLog::DEBUG("上传下载完成，文件 ID: {}, 请执行命令 'md5sum makefile makefile.download1' 来验证文件一致性", *id);
    return *id; 
}

std::string bufferTest() {
    // 基于内存缓冲区的上传下载测试
    // 1. 从本地文件读取内容到内存缓冲区
    std::string content;
    bool ret = viewUtil::FileUtil::Read("./makefile", &content);
    if(!ret) {
        viewLog::ERROR("读取文件失败");
        abort();
    }
    // 2. 将内存缓冲区内容上传到 fastdfs 服务器
    auto id = viewFdfs::FdfsClient::UploadFromBuff(content);
    if(!id) {
        viewLog::ERROR("上传文件失败");
        abort();
    }
    // 3. 从服务器下载文件内容到新的内存缓冲区
    std::string downloadedContent;
    ret = viewFdfs::FdfsClient::DownloadToBuff(*id, &downloadedContent);
    if(!ret) {
        viewLog::ERROR("下载文件失败");
        abort();
    }
    // 4. 比较原始内容和下载内容是否一致
    if(content == downloadedContent) {
        viewLog::INFO("内存缓冲区上传下载测试成功，文件 ID: {}", *id);
    } else {
        viewLog::ERROR("内存缓冲区上传下载测试失败，内容不一致");
        abort();
    }
    return *id;
}

void removeTest(std::vector<std::string>& arr) {
    // 删除测试
    for (auto& id : arr) {
        bool ret = viewFdfs::FdfsClient::Remove(id);
        if(ret) {
            viewLog::INFO("删除文件成功，文件 ID: {}", id);
        } else {
            viewLog::ERROR("删除文件失败，文件 ID: {}", id);
        }
    }
}

int main() {
    viewLog::init_logger();
    viewFdfs::FdfsSettings settings;
    settings.trackerServers = {"192.168.10.129:22122"};
    viewFdfs::FdfsClient::Init(settings);
    std::vector<std::string> fileIds;
    fileIds.push_back(fileTest());
    fileIds.push_back(bufferTest());
    removeTest(fileIds);
    viewFdfs::FdfsClient::Destroy();
    return 0;

}