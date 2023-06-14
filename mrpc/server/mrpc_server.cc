#include<mrpc/server/mrpc_server.h>

namespace mrpc
{

RpcServer::RpcServer(RpcServerOptions option
                    , const tcp::endpoint& endpoint)
    : _option(option)
{
    _is_running.store(false);
    Start(endpoint);
}

RpcServer::~RpcServer()
{
    Stop();
}

void RpcServer::Start(const tcp::endpoint& endpoint)
{
    _is_running.store(true);
    _io_service_pool.reset(new ThreadGroup(_option.work_thread_num, "io server thread group", _option.init_func, _option.end_func));
    
    _listener_ptr.reset(new Listener(_io_service_pool->GetService(), endpoint));
    _listener_ptr->SetAcceptCallback(std::bind(&RpcServer::OnAccept, shared_from_this(), std::placeholders::_1));
    _listener_ptr->SetCreateCallback(std::bind(&RpcServer::OnCreate, shared_from_this(), std::placeholders::_1));
    _listener_ptr->StartListen();
}

void RpcServer::Stop()
{
    if(_is_running.load() == false)
    {
        return;
    }
    _is_running.store(false);
    _io_service_pool->Stop();
    _io_service_pool.reset();
    _listener_ptr->Stop();
    _listener_ptr.reset();
    for(auto iter = _stream_set.begin(); iter != _stream_set.end(); iter++)
    {
        iter->get()->Close("RpcServer destructed");
    }
    _stream_set.clear();
}


void RpcServer::OnAccept(const RpcServerStreamPtr& stream)
{
    if(_is_running.load() == false)
    {
        return;
    }
    stream->UpdateRemote();
    stream->SetConnected(); // 设置为已连接开始收发数据
    LOG(INFO, "accept connect from: %s", EndPointToString(stream->GetRemote()).c_str());
    {
        std::lock_guard<std::mutex> lock(_stream_set_mutex);
        _stream_set.insert(stream);
    }
}

void RpcServer::OnReceive(const RpcServerStreamPtr& stream, RpcRequest request)
{ 
    // 开始解析request
    request.Parse(stream, _service_pool);
}

void RpcServer::OnClose(const RpcServerStreamPtr& stream)
{

}

void RpcServer::OnCreate(const RpcServerStreamPtr& stream)
{
    stream->SetReceiveCallBack(std::bind(&RpcServer::OnReceive, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    stream->SetReceiveCallBack(std::bind(&RpcServer::OnClose, shared_from_this(), 
                                std::placeholders::_1));
}

}