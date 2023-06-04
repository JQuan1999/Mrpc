#include<mrpc/controller/rpc_controller.h>

namespace mrpc{

RpcController::RpcController()
    : _failed(false)
    , _is_sync(false)
{
    _done.store(false);
}

RpcController::~RpcController()
{

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
    return _reason;
}

void RpcController::StartCancel()
{
    // Todo
}

void RpcController::SetFailed(const std::string& reason)
{
    _reason = reason;
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
    _is_signal = false;
}

bool RpcController::IsSync()
{
    return _is_sync;
}

void RpcController::SetRemoteEndPoint(const boost::asio::ip::tcp::endpoint& endpoint)
{
    _remote_endpoint = endpoint;
}

void RpcController::SetRequestMessage(std::string msg)
{
    _request_message = msg;
}

std::string RpcController::GetRequestMessage()
{
    return _request_message;
}

const tcp::endpoint RpcController::GetRemoteEndPoint()
{
    return _remote_endpoint;
}

void RpcController::Wait()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while(!_is_signal){
        _cond.wait(lock);
    }
    _is_signal = false;
}

void RpcController::Signal()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _is_signal = true;
        _cond.notify_one(); // 唤醒阻塞的线程
    }
}

void RpcController::StartTime()
{
    // Todo
}

// callback函数签名 void(RpcControllerPtr)
void RpcController::PushDoneCallBack(callback func)
{
    if(func == nullptr)
    {
        LOG(ERROR, "PushDoneCallBack(): callback func is null");
        return;
    }
    _callback_queue.push_back(func);
}

void RpcController::SetSequenceId(uint64_t id)
{
    _sequence_id = id;
}

uint64_t RpcController::GetSequenceId()
{
    return _sequence_id;
}

void RpcController::Done(std::string msg, bool is_failed = false)
{
    // Done只会被调用一次 已经done直接返回
    if(IsDone())
    {
        return;
    }
    _reason = msg;
    _done.store(true);
    _failed = is_failed;
    while(!_callback_queue.empty()){
        auto func = _callback_queue.front();
        func(shared_from_this());
        _callback_queue.pop_front();
    }
}

bool RpcController::IsDone()
{
    return _done.load();
}

}