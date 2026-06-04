#include <viewEtcd.h>
#include <viewLog.h>
#include <iostream>

int main() {
    viewLog::init_logger();
    //进行服务注册
    const std::string url = "http://127.0.0.1:2379";
    const std::string svc_name = "user";
    const std::string svc_addr = "192.168.10.129:9000";
    // 1. 实例化服务提供者对象
    auto provider = std::make_shared<viewEtcd::ServiceRegister>(url, svc_name, svc_addr);
    // 2. 注册服务即可
    provider->RegisterService();
    getchar();
    return 0;
}