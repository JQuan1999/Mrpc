#include <mrpc/server/mrpc_server.h>
#include <mrpc/common/rpc_controller.h>
#include "echo.pb.h"

using namespace mrpc;
using namespace EchoTest;

class EchoServiceImpl: public EchoServer
{
public:
    EchoServiceImpl(){};
    virtual ~EchoServiceImpl(){};

    virtual void Echo(::google::protobuf::RpcController* controller,
                       const ::EchoTest::EchoRequest* request,
                       ::EchoTest::EchoResponse* response,
                       ::google::protobuf::Closure* done)
    {
        RpcController* cnt = dynamic_cast<RpcController*>(controller);
        cnt->SetSuccess("call success from server");
        response->set_response(request->request() + " from server");
        done->Run();
    }
};

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    RpcServerPtr server_ptr(new RpcServer());
    tcp::endpoint endpoint(address::from_string("127.0.0.1"), 12345);
    EchoServiceImpl* impl = new EchoServiceImpl();

    server_ptr->Start(endpoint);
    if(!server_ptr->RegisterService(impl))
    {
        LOG(ERROR, "register service failed");
        return -1;
    }
    server_ptr->Run();

    server_ptr->Stop();
}