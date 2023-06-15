#include<mrpc/server/mrpc_server.h>

namespace mrpc
{
bool RpcServer::_quit = false;

RpcServer::RpcServer(RpcServerOptions option)
    : _option(option)
    , _is_running(false)
    , _service_pool(new ServicePool())
{
    
}

RpcServer::~RpcServer()
{
    Stop();
}

void RpcServer::Start(const tcp::endpoint& endpoint)
{
    if(_is_running.load() == true)
    {
        return;
    }
    _is_running.store(true);
    _io_service_group.reset(new ThreadGroup(_option.work_thread_num, "io server thread group", _option.init_func, _option.end_func));
    
    _listener_ptr.reset(new Listener(_io_service_group->GetService(), endpoint));
    _listener_ptr->SetAcceptCallback(std::bind(&RpcServer::OnAccept, shared_from_this(), std::placeholders::_1));
    _listener_ptr->SetCreateCallback(std::bind(&RpcServer::OnCreate, shared_from_this(), std::placeholders::_1));
    _listener_ptr->StartListen();
}

void RpcServer::Run()
{
    signal(SIGINT, &RpcServer::SignalHandler);
    signal(SIGQUIT, &RpcServer::SignalHandler);
    _quit = false;
    while(!_quit){
        sleep(1);
    }
}

void RpcServer::Stop()
{
    if(_is_running.load() == false)
    {
        return;
    }
    _is_running.store(false);
    _io_service_group->Stop();
    _io_service_group.reset();
    _listener_ptr->Stop();
    _listener_ptr.reset();
    for(auto iter = _stream_set.begin(); iter != _stream_set.end(); iter++)
    {
        iter->get()->Close("RpcServer destructed");
    }
    _stream_set.clear();
}

bool RpcServer::RegisterService(google::protobuf::Service* service, bool ownship)
{
    return _service_pool->RegisterService(service, ownship);
}

void RpcServer::SignalHandler(int)
{
    _quit = true;
}

void RpcServer::OnAccept(const RpcServerStreamPtr& stream)
{
    if(_is_running.load() == false)
    {
        return;
    }
    stream->UpdateRemote();
    stream->SetConnected(); // 设置为已连接开始收发数据
    LOG(INFO, "OnAccept(): accept connect from: [%s]", EndPointToString(stream->GetRemote()).c_str());
    {
        std::lock_guard<std::mutex> lock(_stream_set_mutex);
        _stream_set.insert(stream);
    }
}

void RpcServer::OnReceive(const RpcServerStreamPtr& stream, RpcRequest request)
{
    // 解析request
    request.Parse(stream, _service_pool);
}

void RpcServer::OnClose(const RpcServerStreamPtr& stream)
{
    std::lock_guard<std::mutex> lock(_stream_set_mutex);
    if(!_stream_set.count(stream))
    {
        LOG(ERROR, "OnClose(): stream is not in stream_set");
        return;
    }
    LOG(DEBUG, "OnClose(): remote [%s] stream is cloesd", EndPointToString(stream->GetRemote()).c_str());
    _stream_set.erase(stream);
}

void RpcServer::OnCreate(const RpcServerStreamPtr& stream)
{
    LOG(DEBUG, "OnCreate(): set stream on receive and on close hook function");
    stream->SetReceiveCallBack(std::bind(&RpcServer::OnReceive, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    stream->SetCloseCallback(std::bind(&RpcServer::OnClose, shared_from_this(), 
                                std::placeholders::_1));
}

}