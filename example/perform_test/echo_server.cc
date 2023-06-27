#include<cstdio>
#include<mrpc/server/mrpc_server.h>
#include<time.h>
#include<signal.h>
#include<unistd.h>
#include<atomic>

#include "echo.pb.h"

using namespace mrpc;

static bool quit = false;
static std::atomic<long> s_succed_count(0);

class ServiceImpl: public PerfTest::EchoService{
public:
    ServiceImpl(){}
    virtual ~ServiceImpl(){}
    
    virtual void Echo(::google::protobuf::RpcController* controller,
                       const ::PerfTest::Request* request,
                       ::PerfTest::Response* response,
                       ::google::protobuf::Closure* done)
    {
        ((RpcController*)controller)->SetSuccess("success");
        response->set_res(request->req());
        done->Run();
        s_succed_count++;
    }
};


static void SignalHandler(int)
{
    quit = true;
}

int main(int argc, char* argv[])
{
    if(argc < 4){
        fprintf(stderr, "Usage: %s <host> <port> <thread_num>\n", argv[0]);
        return -1;
    }
    MRPC_SET_LOG_LEVEL(INFO);
    RpcServerOptions options;
    options.work_thread_num = atoi(argv[3]);
    RpcServerPtr server(new RpcServer(options));
    ServiceImpl* impl = new ServiceImpl();

    server->RegisterService(impl);

    std::string host(argv[1]);
    int port = atoi(argv[2]);
    if(!server->Start(host, port))
    {
        LOG(ERROR, "Start server failed");
        return -1;
    }

    signal(SIGQUIT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    struct timeval tv1, tv2;
    long interval_us = 1000000;
    long elapsed_time;
    gettimeofday(&tv1, nullptr);
    long last_succed_count = 0;
    while(!quit)
    {
        usleep(interval_us / 10);

        gettimeofday(&tv2, nullptr);
        elapsed_time = (tv2.tv_sec - tv1.tv_sec) * interval_us + tv2.tv_usec - tv1.tv_usec;
        // 1秒打印一次
        if(elapsed_time >= interval_us){
            long curr_succed_count = s_succed_count.load() - last_succed_count;
            LOG(INFO, "QPS=%lld", curr_succed_count); // 打印1s的qps
            last_succed_count = s_succed_count.load();
            gettimeofday(&tv1, nullptr);
        }
    }
    
    server->Stop();

    return 0;
}