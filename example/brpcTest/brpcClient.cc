#include <viewRpc.h>
#include "cal.pb.h"
#include <memory>
#include <iostream>

int main() {
    // 1. 实例化服务节点管理对象
    view::RpcManager rpcManager;
    rpcManager.CareService("user");
    rpcManager.AddNode("user", "192.168.10.129:9000");
    
    // 2. 获取节点对应的 channel
    auto channel = rpcManager.GetNode("user");
    if (!channel) {
        std::cerr << "get channel failed" << std::endl;
        return -1;
    }
    
    // 3. 使用智能指针管理资源
    auto cntl = new brpc::Controller();
    auto req = new cal::AddReq();
    auto rsp = new cal::AddRsp();
    
    req->set_num1(10);
    req->set_num2(20);
    
    // 4. 构造 closure，捕获智能指针（按值移动）
    auto closure = view::ClosureFactory::Create([=](){
        std::unique_ptr<brpc::Controller> cntl_guard(cntl);
        std::unique_ptr<cal::AddReq> req_guard(req);
        std::unique_ptr<cal::AddRsp> rsp_guard(rsp);
        if (cntl_guard->Failed() == true) {
            std::cout << "rpc请求失败: " << cntl_guard->ErrorText() << std::endl;
            return ;
        }
        std::cout << rsp_guard->result() << std::endl;
    });
    
    // 5. 发起 RPC 调用
    cal::CalService_Stub stub(channel.get());
    stub.Add(cntl, req, rsp, closure);
    std::cout << "====================================" << std::endl;
    
    // 6. 等待异步调用完成
    getchar();
    
    return 0;
}