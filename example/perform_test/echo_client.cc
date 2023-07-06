#include <mrpc/client/rpc_client_stream.h>
#include <mrpc/client/simple_rpc_channel.h>
#include <atomic>
#include <signal.h>
#include <google/protobuf/service.h>
#include "echo.pb.h"

using namespace mrpc;
using namespace PerfTest;

const int max_pending_count = 1000;
static std::atomic<bool> is_running(true);
static std::atomic<bool> should_wait(false);
static std::atomic<long> pending_count(0);
static std::atomic<long> succeed_count(0);

EchoService_Stub* stub;
Request* request;

extern void DoneCallback(RpcControllerPtr cnt);

void StartCall()
{
    RpcControllerPtr cnt(new RpcController());
    PerfTest::Response* res = new PerfTest::Response();
    cnt->SetResponse(res);
    cnt->SetRequest(request);
    google::protobuf::Closure* done = google::protobuf::NewCallback(DoneCallback, cnt);
    if(++pending_count >= max_pending_count)
    {
        should_wait = true;
    }
    stub->Echo(cnt.get(), request, res, done);
}

void DoneCallback(RpcControllerPtr cnt)
{
    if(--pending_count < max_pending_count)
    {
        should_wait = false;
    }
    Request* req = dynamic_cast<Request*>(cnt->GetRequest());
    Response* res = dynamic_cast<Response*>(cnt->GetResponse());
    // Todo 统计超时失败的个数
    if(cnt->Failed())
    {
        is_running = false;
        LOG(ERROR, "request failed: %s", cnt->ErrorText().c_str());
    }else if(res->res().size() != req->req().size()){
        is_running = false;
        LOG(ERROR, "request failed: response not matched");
    }else{
        ++succeed_count;
    }
    delete res;
    if(is_running && !should_wait){
        StartCall();
    }
}

void SignalHandler(int){
    is_running = false;
}

int main(int argc, char* argv[])
{
    if(argc < 4){
        fprintf(stderr, "Usage: %s <host> <port> <message_size> [is_grace_quit]\n", argv[0]);
        return -1;
    }
    std::string host(argv[1]);
    int port = atoi(argv[2]);
    int message_size = atoi(argv[3]);
    std::string msg;
    msg.resize(message_size, 'c');
    bool is_grace_quit = false;
    if(argc > 4){
        if(strcmp(argv[4], "true") == 0){
            is_grace_quit = true;
        }else if(strcmp(argv[4], "false") == 0){
            is_grace_quit = false;
        }else{
            fprintf(stderr, "Invalid param 'is_grace_quit': should be true or false");
            return -1;
        }
    }
    MRPC_SET_LOG_LEVEL(NOTICE);

    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    RpcClientOptions option;
    option.work_thread_num = 4;
    option.callback_thread_num = 4;
    RpcClientPtr client(new RpcClient(option));
    SimpleChannelPtr channel(new RpcSimpleChannel(client, host, port));
    if(!channel->ResovleSuccess())
    {
        LOG(ERROR, "resovle host failed");
        client->Stop();
        return -1;
    }

    request = new Request();
    request->set_req(msg);
    stub = new EchoService_Stub(channel.get());
    
    long interval_us = 1000000;
    long last_succced_count = 0;
    struct timeval tv1, tv2;
    gettimeofday(&tv1, nullptr);
    while(is_running){
        StartCall();

        if(should_wait){
            usleep(interval_us/10);
        }

        gettimeofday(&tv2, nullptr);
        long elapsed_time = (tv2.tv_sec - tv1.tv_sec) * interval_us + (tv2.tv_usec - tv1.tv_usec);
        if(elapsed_time > interval_us){
            LOG(NOTICE, "QPS:%lld, pending count=%lld", succeed_count - last_succced_count, pending_count.load());
            last_succced_count = succeed_count.load();
            gettimeofday(&tv1, nullptr);
        }
    }

    // 等待剩余回调函数执行完毕再退出
    if(is_grace_quit){
        LOG(NOTICE, "gracely exiting...");
        while(pending_count > 0){
            LOG(NOTICE, "pending count=%lld", pending_count.load());
            usleep(interval_us);
        }
    }
    client->Stop();
    delete request;
    delete stub;

    fprintf(stderr, "Succced: %ld", succeed_count.load());
    return 0;
}