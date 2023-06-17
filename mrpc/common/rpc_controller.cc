#include<mrpc/common/rpc_controller.h>

namespace mrpc{

RpcController::RpcController()
    : _failed(false)
    , _is_sync(false)
    , _done(false)
    , _callback(nullptr)
    , _remote_reason("")
    , _local_reason("")
{

}

RpcController::~RpcController()
{
    LOG(DEBUG, "in ~RpcController() address: %p", this);
}

void RpcController::Reset()
{

}

bool RpcController::Failed() const
{
    return _failed;
}

std::string RpcController::ErrorText() const
{
    std::string remote_reason = "remote reason: " + (_remote_reason.size() == 0 ? "empty" : _remote_reason);
    std::string local_reason = "local reason: " + (_local_reason.size() == 0 ? "empty" : _local_reason);
    return remote_reason + " " + local_reason;
}

void RpcController::StartCancel()
{
    // Todo
}

const std::string& RpcController::RemoteReason() const
{
    return _remote_reason;
}

void RpcController::SetRemoteReason(std::string reason)
{
    _remote_reason = reason;
}

const std::string& RpcController::LocalReason() const
{
    return _local_reason;
}

void RpcController::SetFailed(const std::string& reason)
{
    _remote_reason = reason;
    _failed = true;
}

void RpcController::SetSuccess(const std::string& reason)
{
    _remote_reason = reason;
    _failed = false;
}

bool RpcController::IsCanceled() const
{
    // Todo
    return false;
}

void RpcController::NotifyOnCancel(google::protobuf::Closure* callback)
{
    // Todo
}

void RpcController::SetMethodName(const std::string method_name)
{
    _method_name = method_name;
}

const std::string& RpcController::GetMethodName()
{
    return _method_name;
}

void RpcController::SetServiceName(const std::string service_name)
{
    _service_name = service_name;
}

const std::string& RpcController::GetServiceName()
{
    return _service_name;
}

void RpcController::SetSync()
{
    _is_sync = true;
}

bool RpcController::IsSync()
{
    return _is_sync;
}

void RpcController::SetRemoteEndPoint(const boost::asio::ip::tcp::endpoint& endpoint)
{
    _remote_endpoint = endpoint;
}

void RpcController::SetSendMessage(const ReadBufferPtr& sendbuf)
{
    _send_buf = sendbuf;
}

ReadBufferPtr& RpcController::GetSendMessage()
{
    return _send_buf;
}

const tcp::endpoint RpcController::GetRemoteEndPoint()
{
    return _remote_endpoint;
}

void RpcController::Wait()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while(!_done.load())
    {
        _cond.wait(lock);
    }
}

void RpcController::Signal()
{
    _cond.notify_one(); // 唤醒阻塞的线程
}

void RpcController::StartTime()
{
    // Todo
}

// callback函数签名 void(RpcControllerPtr)
void RpcController::SetDoneCallBack(callback func)
{
    if(func == nullptr)
    {
        LOG(ERROR, "PushDoneCallBack(): callback func is null");
        return;
    }
    _callback = func;
}

void RpcController::SetSequenceId(uint64_t id)
{
    _sequence_id = id;
}

uint64_t RpcController::GetSequenceId()
{
    return _sequence_id;
}

void RpcController::Done(std::string reason, bool failed)
{
    // Done只会被调用一次 已经done直接返回
    if(IsDone())
    {
        return;
    }
    _local_reason = reason;
    _failed = failed;
    _done.store(true);
    if(_callback)
    {
        _callback(shared_from_this());
    }
}

void RpcController::SetSeverStream(const RpcServerStreamPtr& server_stream)
{
    _server_stream = server_stream;
}

RpcServerStreamPtr RpcController::GetSeverStream()
{
    return _server_stream;
}

void RpcController::SetRequest(google::protobuf::Message* request)
{
    _request = request;
}

google::protobuf::Message* RpcController::GetRequest()
{
    return _request;
}

void RpcController::SetResponse(google::protobuf::Message* response)
{
    _response = response;
}

google::protobuf::Message*RpcController::GetResponse()
{
    return _response;
}

bool RpcController::IsDone()
{
    return _done.load();
}

}