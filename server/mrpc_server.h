#ifndef _MRPC_SERVER_H
#define _MRPC_SERVER_H

#include<mutex>
#include<atomic>
#include<set>
#include<memory>
#include<string>

#include<mrpc/common/end_point.h>
#include<mrpc/common/thread_group.h>
#include<mrpc/server/listener.h>
#include<mrpc/server/service_pool.h>

namespace mrpc{

struct RpcServerOptions
{
    int work_thread_num;
    ThreadFunc init_func;
    ThreadFunc end_func;
    
    RpcServerOptions():
    work_thread_num(4),
    init_func(nullptr),
    end_func(nullptr)
    {}
};

class RpcServer;
typedef std::shared_ptr<RpcServer> RpcServerPtr;

class RpcServer: public std::enable_shared_from_this<RpcServer>
{
public:
    RpcServer(RpcServerOptions option = RpcServerOptions()
             , const tcp::endpoint& endpoint);
    ~RpcServer();
    void Start(const tcp::endpoint& endpoint);
    void Stop();

private:
    void OnCreate(const RpcServerStreamPtr& stream);

    void OnAccept(const RpcServerStreamPtr& stream);

    void OnReceive(const RpcServerStreamPtr& stream, RpcRequest request);

    void OnClose(const RpcServerStreamPtr& stream);

private:
    ListenerPtr _listener_ptr;
    ServicePoolPtr _service_pool;
    std::atomic<bool> _is_running;
    RpcServerOptions _option;
    ThreadGroupPtr _io_service_pool; // io_service线程组
    std::set<RpcServerStreamPtr> _stream_set; // server_stream集合
    std::mutex _stream_set_mutex;
};

}

#endif
