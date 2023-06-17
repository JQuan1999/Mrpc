#include <mrpc/server/mrpc_server.h>
#include <mrpc/common/rpc_controller.h>
#include "test.pb.h"

using namespace test;
using namespace mrpc;

class ServiceImpl: public UserService
{
public:
    ServiceImpl(){};
    virtual ~ServiceImpl(){

    }
    virtual void Tolower(::google::protobuf::RpcController* controller,
                       const ::test::Request* request,
                       ::test::Response* response,
                       ::google::protobuf::Closure* done)
    {
        RpcController* cnt = dynamic_cast<RpcController*>(controller);
        cnt->SetSuccess("Tolower method is called sucesss");
        std::string str = request->request();
        LOG(INFO, "Tolower(): request message from: %s message: %s", 
            EndPointToString(cnt->GetRemoteEndPoint()).c_str(), str.c_str());
        for(auto& ch: str){
            ch = tolower(ch);
        }
        response->set_response(str);
        done->Run();
    }

    virtual void ToUpper(::google::protobuf::RpcController* controller,
                       const ::test::Request* request,
                       ::test::Response* response,
                       ::google::protobuf::Closure* done)
    {
        RpcController* cnt = dynamic_cast<RpcController*>(controller);
        cnt->SetSuccess("ToUpper method is called sucesss");
        std::string str = request->request();
        LOG(INFO, "ToUpper(): request message from: %s message: %s", 
            EndPointToString(cnt->GetRemoteEndPoint()).c_str(), str.c_str());
        for(auto& ch: str){
            ch = toupper(ch);
        }
        response->set_response(str);
        done->Run();
    }
    virtual void Expand(::google::protobuf::RpcController* controller,
                       const ::test::Request* request,
                       ::test::Response* response,
                       ::google::protobuf::Closure* done)
    {
        RpcController* cnt = dynamic_cast<RpcController*>(controller);
        std::string str = request->request();
        int times;
        if(request->has_times())
        {
            times = request->times();
            cnt->SetSuccess("Expand method is called sucesss");
            LOG(INFO, "Expand(): request message from: %s message: %s", 
            EndPointToString(cnt->GetRemoteEndPoint()).c_str(), str.c_str());
            std::string ret;
            while(times--)
            {
                ret += str;
            }
            response->set_response(ret);
        }else{
            cnt->SetFailed("Expand method failed because expand times is not setted");
        }
        done->Run();
    }
};

void Init()
{
    LOG(INFO, "Init thread succeed");
}

void End()
{
    LOG(INFO, "Destory thread succeed");
}

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    RpcServerOptions option;
    option.work_thread_num = 4;
    option.init_func = std::bind(Init);
    option.end_func = std::bind(End);

    RpcServerPtr server_ptr(new RpcServer(option));
    tcp::endpoint endpoint(address::from_string("127.0.0.1"), 12345);
    ServiceImpl* impl = new ServiceImpl();

    server_ptr->Start(endpoint);
    if(!server_ptr->RegisterService(impl))
    {
        LOG(ERROR, "register service failed");
        return -1;
    }
    server_ptr->Run();

    server_ptr->Stop();
}