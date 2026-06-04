#include <viewEtcd.h>
#include <viewLog.h>
#include <iostream>

void online(const std::string &svc_name, const std::string &svc_addr) {
    viewLog::INFO("新服务 {} 上线节点: {}", svc_name, svc_addr);
}
void offline(const std::string &svc_name, const std::string &svc_addr) {
    viewLog::INFO("新服务 {} 下线节点: {}", svc_name, svc_addr);
}

int main()
{
    viewLog::init_logger();
    //进行服务注册
    const std::string url = "http://127.0.0.1:2379";
    // 1. 实例化服务监控者对象
    viewEtcd::ServiceDiscovery watcher(url, online, offline);
    // 2. 发现服务即可
    watcher.WatchService();
    getchar();
    return 0;
}