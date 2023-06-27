#include<mrpc/client/mrpc_client.h>
#include<mrpc/client/simple_rpc_channel.h>
#include<time.h>
#include"sleep.pb.h"

using namespace TimeoutTest;
using namespace mrpc;

void CallBack(RpcControllerPtr cnt, bool* flag)
{
    Request* req = dynamic_cast<Request*>(cnt->GetRequest());
    Response* res = dynamic_cast<Response*>(cnt->GetResponse());
    if(cnt->Failed())
    {
        LOG(INFO, "call method failed error text: %s", cnt->ErrorText().c_str());
    }
    else
    {
        LOG(INFO, "call method success res: %d", res->return_time());
    }
    *flag = true;
    delete req;
    delete res;
}

int main()
{
    MRPC_SET_LOG_LEVEL(INFO);

    RpcClientPtr client(new RpcClient());
    std::string address = "127.0.0.1";
    int port = 8888;
    SimpleChannelPtr channel(new RpcSimpleChannel(client, address, port));
    if(!channel->ResovleSuccess())
    {
        LOG(ERROR, "channel resovle address failed");
        client->Stop();
        return -1;
    }

    SleepService_Stub* stub = new SleepService_Stub(channel.get());
    RpcControllerPtr cnt(new RpcController());
    Request* req = new Request();
    Response* res = new Response();

    // callmethod 1
    struct timeval tv1, tv2;
    gettimeofday(&tv1, nullptr);
    req->set_sleep_time(3);
    cnt->SetTimeout(2);

    stub->Sleep(cnt.get(), req, res, nullptr);

    gettimeofday(&tv2, nullptr);

    long time = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
    double realtime = time * 1.0 / 1000000;
    if(cnt->Failed())
    {
        LOG(INFO, "call method failed error text: %s cost time: %.6f", cnt->ErrorText().c_str(), realtime);
    }
    else
    {
        LOG(INFO, "call method success cost time: %.6f", realtime);
    }

    // callmethod 2
    bool call_flag = false;
    cnt.reset(new RpcController());
    cnt->SetTimeout(2);
    cnt->SetRequest(req);
    cnt->SetResponse(res);

    req->set_sleep_time(1);
    
    google::protobuf::Closure* done = google::protobuf::NewCallback(CallBack, cnt, &call_flag);

    stub->Sleep(cnt.get(), req, res, done);
    while(!call_flag)
    {
        sleep(1);
    }

    client->Stop();
    delete stub;
    return 0;
}