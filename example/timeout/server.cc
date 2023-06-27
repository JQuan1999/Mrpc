#include<unistd.h>
#include<mrpc/server/mrpc_server.h>
#include"sleep.pb.h"

using namespace mrpc;
using namespace TimeoutTest;

class ServiceImpl: public SleepService
{
public:
    ServiceImpl(){}
    virtual ~ServiceImpl(){}

    virtual void Sleep(::google::protobuf::RpcController* controller,
                       const ::TimeoutTest::Request* request,
                       ::TimeoutTest::Response* response,
                       ::google::protobuf::Closure* done)
    {
        ((RpcController*)controller)->SetSuccess("success");
        int time = request->sleep_time();
        sleep(time);
        response->set_return_time(time);
        done->Run();    
    }
};


int main()
{
    MRPC_SET_LOG_LEVEL(INFO);
    RpcServerPtr server(new RpcServer());
    std::string host = "127.0.0.1";
    int port = 8888;
    ServiceImpl* impl = new ServiceImpl();
    if(!server->RegisterService(impl))
    {
        LOG(ERROR, "Register service failed");
        return -1;
    }
    if(!server->Start(host, port))
    {
        LOG(ERROR, "server start failed");
        return -1;
    }
    server->Run();
    
    server->Stop();
    return 0;
}