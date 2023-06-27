#include<mrpc/client/mrpc_client.h>

namespace mrpc
{

RpcClient::RpcClient(RpcClientOptions option)
    : _option(option)
    , _next_request_id(0)
    , _is_running(false)
{
    Start();
}

RpcClient::~RpcClient()
{
    Stop();
}


void RpcClient::Start()
{
    if(_is_running.load() == true)
    {
        return;
    }
    _is_running.store(true);

    _timer_thread_group.reset(new ThreadGroup(_option.timer_thread_num, "timer_thread_group", _option.init_func, _option.end_func));

    _timeout_ptr.reset(new TimeoutManager(_timer_thread_group->GetService()));
    _timeout_ptr->Start();
    
    _work_thread_group.reset(new ThreadGroup(_option.work_thread_num, "client_work_thread_group", _option.init_func, _option.end_func));
    
    _callback_group.reset(new ThreadGroup(_option.callback_thread_num, "client_callback_thread_group", _option.init_func, _option.end_func));
}

IoContext& RpcClient::GetIoService()
{
    return _work_thread_group->GetService();
}

ThreadGroupPtr RpcClient::GetCallBackGroup()
{
    return _callback_group;
}

void RpcClient::Stop()
{
    if(!_is_running.load()){
        return;
    }
    _is_running.store(false);
    {
        std::lock_guard<std::mutex> lock(_stream_map_mutex);
        for(auto& p: _stream_map)
        {
            p.second->Close("client stopped");
        }
        _stream_map.clear();
    }
    _timer_thread_group->Stop();
    _timer_thread_group.reset();
    _timeout_ptr->Stop();
    _timeout_ptr.reset();
    _work_thread_group->Stop();
    _work_thread_group.reset();
    _callback_group->Stop();
    _callback_group.reset();
}


void RpcClient::CallMethod(const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          RpcController* cnt)
{
    if(!_is_running)
    {
        LOG(INFO, "CallMethod(): client is not running, ingore");
        cnt->Done("Client is not running, should start it first", true);
        return;
    }
    // 1.检查endpoint对应的rpc_stream是否存在 或创建新的rpc_stream
    tcp::endpoint remote_endpoint = cnt->GetRemoteEndPoint();
    auto stream_ptr = FindOrCreateStream(remote_endpoint);

    // 2.1 设置rpc协议头部 2.2 设置rpc_meta控制信息 2.3 将request序列化
    cnt->SetSequenceId(GenerateSequenceId());
    WriteBufferPtr writebuf_ptr(new WriteBuffer());
    
    RpcHeader header;
    int header_size = sizeof(header);
    int pos = writebuf_ptr->Reserve(header_size);

    RpcMeta meta;
    meta.set_type(RpcMeta::REQUEST); // 设置为request类型
    meta.set_sequence_id(cnt->GetSequenceId()); // 设置本次request id
    meta.set_service(cnt->GetServiceName());
    meta.set_method(cnt->GetMethodName());

    if(!meta.SerializeToZeroCopyStream(writebuf_ptr.get()))
    {
        LOG(ERROR, "CallMethod(): %s: serialize rpc meta failed", EndPointToString(cnt->GetRemoteEndPoint()).c_str());
        cnt->Done("serialized rpc meta data failed", true);
        return;
    }
    
    int meta_size = writebuf_ptr->ByteCount() - pos - header_size; // meta size
    
    if(!request->SerializeToZeroCopyStream(writebuf_ptr.get()))
    {
        LOG(ERROR, "CallMethod(): %s: serialize request failed", EndPointToString(cnt->GetRemoteEndPoint()).c_str());
        cnt->Done("serialized request data failed", true);
        return;
    }
    int data_size = writebuf_ptr->ByteCount() - pos - header_size - meta_size;

    header.meta_size = meta_size;
    header.data_size = data_size;
    header.message_size = meta_size + data_size;

    // 插入header
    writebuf_ptr->SetData(pos, reinterpret_cast<char*>(&header), header_size);
    ReadBufferPtr readbuf(new ReadBuffer());
    writebuf_ptr->SwapOut(readbuf.get());

    cnt->SetSendMessage(readbuf);
    cnt->SetResponse(response);
    if(cnt->GetTimeout() > 0)
    {
        _timeout_ptr->Add(cnt->shared_from_this());
    }
    // 3. 调用stream将数据发送给server端
    stream_ptr->CallMethod(cnt->shared_from_this());
}

void RpcClient::EraseStream(const RpcClientStreamPtr& stream)
{
    if(!_is_running)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(_stream_map_mutex);
    tcp::endpoint endpoint = stream->GetRemote();
    if(!_stream_map.count(endpoint))
    {
        return;
    }
    if(!_stream_map[endpoint]->IsClosed()){
        return;
    }
    _stream_map.erase(endpoint);
}

// 找到endpoint对应的stream或创建新的stream并保存在map中
RpcClientStreamPtr RpcClient::FindOrCreateStream(const tcp::endpoint& endpoint)
{
    std::lock_guard<std::mutex> lock(_stream_map_mutex);
    if(_stream_map.count(endpoint))
    {
        return _stream_map[endpoint];
    }
    else
    {
        RpcClientStreamPtr stream = std::make_shared<RpcClientStream>(_work_thread_group->GetService(), endpoint);
        stream->SetNoDelay(_option.no_delay);
        stream->SetCloseCallback(std::bind(&RpcClient::EraseStream, shared_from_this(), std::placeholders::_1));
        _stream_map[endpoint] = stream;
        stream->AsyncConnect();
        return stream;
    }
}

uint64_t RpcClient::GetSequenceId()
{
    return _next_request_id.load();
}

uint64_t RpcClient::GenerateSequenceId()
{
    ++_next_request_id;
    return _next_request_id.load();
}

}