#include <viewRpc.h>
#include "cal.pb.h"
#include <memory>
#include <iostream>

int main() {
    // 1. 实例化服务节点管理对象
    viewRpc::RpcManager rpcManager;
    rpcManager.CareService("user");
    rpcManager.AddNode("user", "192.168.10.129:9000");
    
    // 2. 获取节点对应的 channel
    auto channel = rpcManager.GetNode("user");
    if (!channel) {
        std::cerr << "get channel failed" << std::endl;
        return -1;
    }
    
    // 3. 用 unique_ptr 管理资源，先取裸指针供 brpc 使用
    auto cntl = std::make_unique<brpc::Controller>();
    auto req  = std::make_unique<cal::AddReq>();
    auto rsp  = std::make_unique<cal::AddRsp>();

    req->set_num1(10);
    req->set_num2(20);

    // brpc API 使用裸指针
    brpc::Controller* cntl_raw = cntl.get();
    cal::AddReq*      req_raw  = req.get();
    cal::AddRsp*      rsp_raw  = rsp.get();

    // 4. 构造 closure，移动 unique_ptr 移交生命周期管理
    auto closure = viewRpc::ClosureFactory::Create(
        [cntl = std::move(cntl), req = std::move(req), rsp = std::move(rsp)]() {
            if (cntl->Failed()) {
                std::cout << "rpc请求失败: " << cntl->ErrorText() << std::endl;
                return;
            }
            std::cout << rsp->result() << std::endl;
        });

    // 5. 发起 RPC 调用
    cal::CalService_Stub stub(channel.get());
    stub.Add(cntl_raw, req_raw, rsp_raw, closure);
    std::cout << "====================================" << std::endl;

    // 6. 等待异步调用完成
    getchar();
    
    return 0;
}