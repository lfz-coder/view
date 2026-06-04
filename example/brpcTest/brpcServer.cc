#include <viewRpc.h>
#include "cal.pb.h"

class CalServiceImpl : public cal::CalService {
    public:
        CalServiceImpl() = default;
        ~CalServiceImpl() = default;
        void Add(::google::protobuf::RpcController* controller,
            const ::cal::AddReq* request,
            ::cal::AddRsp* response,
            ::google::protobuf::Closure* done) override {
            brpc::ClosureGuard done_guard(done);
            response->set_result(request->num1() + request->num2());
        }
        void Hello(::google::protobuf::RpcController* controller,
            const ::cal::helloReq* request,
            ::cal::helloRsp* response,
            ::google::protobuf::Closure* done) override {
            brpc::ClosureGuard done_guard(done);
            brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
            const auto& headers = cntl->http_request();
            std::cout << "Method:" << brpc::HttpMethod2Str(headers.method()) << std::endl;
            std::cout << "Body:" << cntl->request_attachment().to_string() << std::endl;

            cntl->response_attachment().append("回显:" + cntl->request_attachment().to_string());
            cntl->http_response().set_status_code(200);
        }
};

int main() {
    // 使用 shared_ptr 确保 ServerFactory::Create 失败时自动释放
    auto service = std::make_shared<CalServiceImpl>();
    auto server  = viewRpc::ServerFactory::Create(9000, service.get());
    if (!server) {
        std::cerr << "ServerFactory::Create failed" << std::endl;
        return -1;
    }
    // 延长 service 生命周期，防止 brpc 尚未接管时被释放
    // SERVER_OWNS_SERVICE 意味着 brpc 接管后会自行管理
    service.reset(); // 让 brpc 接管所有权
    server->RunUntilAskedToQuit();
    return 0;
}