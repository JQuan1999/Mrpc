#ifndef _MRPC_CLIENT_H_
#define _MRPC_CLIENT_H_

#include<google/protobuf/service.h>
#include<unordered_map>
#include<memory>
#include<mutex>
#include<atomic>

#include<mrpc/common/rpc_controller.h>
#include<mrpc/common/thread_group.h>
#include<mrpc/common/end_point.h>
#include<mrpc/proto/rpc_header.h>
#include<mrpc/proto/rpc_meta.pb.h>
#include<mrpc/client/rpc_client_stream.h>
#include<mrpc/common/timeout_manager.h>

namespace mrpc{

struct RpcClientOptions
{
    int work_thread_num; // 工作线程数目

    int callback_thread_num; // 回调函数线程数目

    int timer_thread_num; // 定时线程数目

    FuncType init_func; // 初始化函数

    FuncType end_func; // 结束函数

    int keep_alive_time; // 保持连接的时间 超过这个时间未进行读写就关闭

    int connect_timeout; // connect超时时间

    bool no_delay; // tcp是否延迟发送

    RpcClientOptions()
        : work_thread_num(4)
        , callback_thread_num(1)
        , timer_thread_num(1)
        , init_func(nullptr)
        , end_func(nullptr)
        , keep_alive_time(-1)
        , connect_timeout(-1)
        , no_delay(true)
    {}
};

class RpcClient;
typedef std::shared_ptr<RpcClient> RpcClientPtr;

class RpcClient: public std::enable_shared_from_this<RpcClient>
{
public:
    explicit RpcClient(RpcClientOptions option = RpcClientOptions());

    ~RpcClient();

    IoContext& GetIoService();

    ThreadGroupPtr GetCallBackGroup();

    void Start();

    void Stop();

    void CallMethod(const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    RpcController* crt);

    uint64_t GetSequenceId();

    uint64_t GenerateSequenceId();

private:
    RpcClient(const RpcClient&);

    RpcClient& operator=(const RpcClient&);

    void EraseStream(const RpcClientStreamPtr& stream);

    RpcClientStreamPtr FindOrCreateStream(const tcp::endpoint& endpoint);

private:
    RpcClientOptions _option;
    std::atomic<uint64_t> _next_request_id; // 表示client下一个发送消息的序列号
    std::atomic<bool> _is_running;
    std::mutex _stream_map_mutex;
    std::map<tcp::endpoint, RpcClientStreamPtr> _stream_map; // endpoint对应一个stream连接
    TimeoutManagerPtr _timeout_ptr;
    ThreadGroupPtr _timer_thread_group;
    ThreadGroupPtr _work_thread_group;
    ThreadGroupPtr _callback_group;
};
}

#endif