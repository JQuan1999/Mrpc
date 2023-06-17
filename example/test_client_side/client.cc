#include"test.pb.h"
#include<mrpc/client/mrpc_client.h>
#include<mrpc/client/simple_rpc_channel.h>

using namespace mrpc;
using namespace test;

void InitFunc()
{
    LOG(INFO, "InitFunc(): thread init succeed");
}

void EndFunc()
{
    LOG(INFO, "EndFunc(): thread destroy succeed");
}

void CallBack()
{

}

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    RpcClientOptions options;
    options.init_func = std::bind(InitFunc);
    options.end_func = std::bind(EndFunc);
    options.work_thread_num = 1;

    RpcClientPtr client(new RpcClient(options));
    std::string address = "127.0.0.1";
    SimpleChannelPtr channle(new RpcSimpleChannel(client, address, 12345));
    UserService_Stub stub(channle.get());

    Request* request = new Request();
    request->set_request("request");
    Response* response = new Response();
    RpcControllerPtr cnt(new RpcController());
    
    // callmethod 1
    stub.ToUpper(cnt.get(), request, response, nullptr);

    if(cnt->Failed())
    {
        LOG(INFO, "request failed: %s", cnt->ErrorText().c_str());
    }else{
        LOG(INFO, "request succeed response: %s", response->response().c_str());
    }

    // callmethod 2
    cnt.reset(new RpcController());
    request->set_request("REQUEST");
    stub.Tolower(cnt.get(), request, response, nullptr);
    if(cnt->Failed())
    {
        LOG(INFO, "request failed: %s", cnt->ErrorText().c_str());
    }else{
        LOG(INFO, "request succeed response: %s", response->response().c_str());
    }

    // callmethod 3
    cnt.reset(new RpcController());
    request->set_request("REQUEST");
    request->set_times(2);
    stub.Expand(cnt.get(), request, response, nullptr);
    if(cnt->Failed())
    {
        LOG(INFO, "request failed: %s", cnt->ErrorText().c_str());
    }else{
        LOG(INFO, "request succeed response: %s", response->response().c_str());
    }

    // callmethod 4 callfailed
    cnt.reset(new RpcController());
    request->set_request("REQUEST");
    request->clear_times();  // not set expand times

    stub.Expand(cnt.get(), request, response, nullptr);
    if(cnt->Failed())
    {
        LOG(INFO, "request failed: %s", cnt->ErrorText().c_str());
    }else{
        LOG(INFO, "request succeed response: %s", response->response().c_str());
    }

    // callmethod 5 async callmethod
    LOG(DEBUG, "channel use count = %d", channle.use_count());
    delete request;
    delete response;
    return 0;
}