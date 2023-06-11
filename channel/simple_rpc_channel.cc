#include<mrpc/channel/simple_rpc_channel.h>

namespace mrpc{

RpcSimpleChannel::RpcSimpleChannel(const RpcClientPtr& rpc_client_ptr, const std::string& address, uint32_t port)
    : _client_ptr(rpc_client_ptr)
    , _address(address)
    , _port(port)
    , _wait_count(0)
    , _resolve_success(false)
{
    Init();
}

RpcSimpleChannel::~RpcSimpleChannel()
{
    Stop();
}

bool RpcSimpleChannel::Init()
{
    if(!ResovleAddress(_client_ptr->GetIoService(), _address, _port, &_remote_endpoint))
    {
        LOG(INFO, "Init(): resolve address failed");
        _resolve_success = false;
        return false;
    }
    else
    {
        LOG(INFO, "Init(): resolve address success, endpoint: [%s]", EndPointToString(_remote_endpoint));
        _resolve_success = true;
        return true;
    }
}

void RpcSimpleChannel::Stop()
{
    LOG(INFO, "Stop(): simple rpc channel stop: %s", _address.c_str());
}

void RpcSimpleChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller,
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf::Closure* done)
{
    ++_wait_count;
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string method_name = method->name();
    std::string service_name = sd->name();

    RpcController* crt = dynamic_cast<RpcController*>(controller);
    crt->SetMethodName(method_name);
    crt->SetServiceName(service_name);
    // cnt设置回调函数 当完成时调用channel的回调函数
    crt->PushDoneCallBack(std::bind(&RpcSimpleChannel::DoneCallBack, shared_from_this(), 
                                    done, std::placeholders::_1));
    
    if(done == nullptr)
    {
        crt->SetSync(); // 设置为同步调用
    }
    if(!_resolve_success)
    {
        LOG(ERROR, "CallMethod(): resolve address failed: %s", _address.c_str());
        crt->Done("solve address failed", true);
        WaitDone(crt);
    }

    crt->SetRemoteEndPoint(_remote_endpoint);
    crt->StartTime(); // ToDo设置开始时间 超时管理
    _client_ptr->CallMethod(request, response, crt);
    WaitDone(crt);
}

uint32_t RpcSimpleChannel::WaitCount()
{
    return _wait_count.load();
}

void RpcSimpleChannel::WaitDone(RpcController* crt)
{
    if(crt->IsSync())
    {
        crt->Wait(); // 等待完成
    }
}

// 回调函数根据同步调用还是异步调用
// 同步调用唤醒阻塞在CallMethod的线程
// 异步调用将done函数加入client的回调线程
void RpcSimpleChannel::DoneCallBack(google::protobuf::Closure* done, RpcControllerPtr crt)
{
    --_wait_count; // 调用个数减一
    if(crt->IsSync())
    {
        crt->Signal(); // 唤醒阻塞的线程
    }
    else
    {
        CHECK(done);
        _client_ptr->GetCallBackGroup()->Post(done); // 将done降入callback的回调函数
    }
}

}
