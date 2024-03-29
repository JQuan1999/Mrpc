#ifndef _MRPC_SERVER_H
#define _MRPC_SERVER_H

#include<mutex>
#include<atomic>
#include<set>
#include<memory>
#include<string>
#include<signal.h>
#include<unistd.h>

#include<mrpc/common/end_point.h>
#include<mrpc/common/thread_group.h>
#include<mrpc/server/listener.h>
#include<mrpc/server/rpc_request.h>
#include<mrpc/server/service_pool.h>

namespace mrpc{

struct RpcServerOptions
{
    int work_thread_num;
    FuncType init_func;
    FuncType end_func;

    RpcServerOptions()
        : work_thread_num(4)
        , init_func(nullptr)
        , end_func(nullptr)
    {
        
    }
};

class RpcServer;
typedef std::shared_ptr<RpcServer> RpcServerPtr;

class RpcServer: public std::enable_shared_from_this<RpcServer>
{
public:
    RpcServer(RpcServerOptions option = RpcServerOptions());

    ~RpcServer();

    bool Start(const std::string& ip, uint32_t port);
    
    void Stop();

    void Run();

    bool RegisterService(google::protobuf::Service* service, bool ownship=true);

private:
    static void SignalHandler(int);

    void OnCreate(const RpcServerStreamPtr& stream);

    void OnAccept(const RpcServerStreamPtr& stream);

    void OnReceive(const RpcServerStreamPtr& stream, RpcRequest request);

    void OnClose(const RpcServerStreamPtr& stream);

private:
    static bool _quit;
    tcp::endpoint _listen_endpoint;
    ListenerPtr _listener_ptr;
    ServicePoolPtr _service_pool;
    std::atomic<bool> _is_running;
    RpcServerOptions _option;
    ThreadGroupPtr _io_service_group; // io_service线程组
    std::set<RpcServerStreamPtr> _stream_set; // server_stream集合
    std::mutex _stream_set_mutex;
};

}

#endif
