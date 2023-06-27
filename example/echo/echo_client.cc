#include <mrpc/client/simple_rpc_channel.h>
#include <mrpc/client/mrpc_client.h>
#include <boost/asio.hpp>
#include <time.h>
#include "echo.pb.h"

using namespace mrpc;
using namespace EchoTest;

void CallBack(RpcControllerPtr cnt)
{
    EchoRequest* request = dynamic_cast<EchoRequest*>(cnt->GetRequest());
    EchoResponse* response = dynamic_cast<EchoResponse*>(cnt->GetResponse());
    CHECK(request);
    CHECK(response);
    if(cnt->Failed())
    {
        LOG(INFO, "CallBack(): async call method failed error text: %s", cnt->ErrorText().c_str());
    }
    else
    {
        LOG(INFO, "CallBack(): async call method succeed response: %s sequence id = %d", response->response().c_str(), cnt->GetSequenceId());
    }
    delete request;
    delete response;
}

void LastCallBack(RpcControllerPtr cnt, bool* flag)
{
    EchoRequest* request = dynamic_cast<EchoRequest*>(cnt->GetRequest());
    EchoResponse* response = dynamic_cast<EchoResponse*>(cnt->GetResponse());
    CHECK(request);
    CHECK(response);
    if(cnt->Failed())
    {
        LOG(INFO, "CallBack(): async call method failed error text: %s", cnt->ErrorText().c_str());
    }
    else
    {
        LOG(INFO, "CallBack(): async call method succeed response: %s sequence id = %d", response->response().c_str(), cnt->GetSequenceId());
    }
    *flag = true;
    delete request;
    delete response;
}

int main(int argc, char* argv[])
{
    MRPC_SET_LOG_LEVEL(INFO);
    if(argc != 3)
    {
        LOG(INFO, "Usage: ./echo_client ip port");
        return -1;
    }
    RpcClientOptions option;
    option.work_thread_num = 4;

    RpcClientPtr client(new RpcClient(option));
    std::string address = std::string(argv[1]);
    int port = atoi(argv[2]);
    SimpleChannelPtr channel(new RpcSimpleChannel(client, address, port));
    EchoServer_Stub stub(channel.get());

    RpcControllerPtr cnt(new RpcController());
    EchoRequest* request = new EchoRequest();
    cnt->SetRequest(request);
    request->set_request("request from client");
    EchoResponse* response = new EchoResponse();

    // callmethod1 同步调用
    stub.Echo(cnt.get(), request, response, nullptr);
    if(cnt->Failed()){
        LOG(INFO, "call method failed error text: %s", cnt->ErrorText().c_str());
    }else{
        LOG(INFO, "call method succeed response: %s", response->response().c_str());
    }
    delete request;
    delete response;
    // callmethod2 10次异步调用
    // coredump NewCallback传RpcControllerPtr&会coredump
    bool flag = false;
    int count = 10;
    while(client->GetSequenceId() < count)
    {
        cnt.reset(new RpcController());
        request = new EchoRequest();
        cnt->SetRequest(request);
        request->set_request("request from client");
        response = new EchoResponse();
        google::protobuf::Closure* done = nullptr;
        if(client->GetSequenceId() < count - 1)
        {
            done = google::protobuf::NewCallback<RpcControllerPtr>(CallBack, cnt);
        }else{
            done = google::protobuf::NewCallback<RpcControllerPtr, bool*>(LastCallBack, cnt, &flag);
        }
        stub.Echo(cnt.get(), request, response, done);
    }
    while(!flag)
    {
        usleep(100);
    }
    client->Stop();
    return 0;
}