#include<mrpc/client/mrpc_client.h>

namespace mrpc{

RpcClient::RpcClient(RpcClientOptions option)
    : _option(option)
    , _is_running(false)
    , _next_request_id(0)
{
    Start();
}

RpcClient::~RpcClient()
{
    Stop();
}


void RpcClient::Start()
{
    std::lock_guard<std::mutex> lock(_start_stop_mutex);
    if(_is_running)
    {
        return; // 已经启动
    }
    _work_thread_group.reset(new ThreadGroup(_option.work_thread_num, "client_work_thread_group"));
    _callback_group.reset(new ThreadGroup(_option.callback_thread_num, "client_callback_thread_group"));
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
    std::lock_guard<std::mutex> lock(_start_stop_mutex);
    if(!_is_running) return;
    _work_thread_group->Stop();
    _callback_group->Stop();
    // 将指针置空
    _work_thread_group.reset();
    _callback_group.reset();
}


void RpcClient::CallMethod(const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          RpcController* crt)
{
    if(!_is_running)
    {
        LOG(INFO, "CallMethod(): client is not running, ingore");
        crt->Done("Client is not running, should start it first", true);
        return;
    }
    // 1.检查endpoint对应的rpc_stream是否存在 或创建新的rpc_stream
    tcp::endpoint remote_endpoint = crt->GetRemoteEndPoint();
    auto stream_ptr = FindOrCreateStream(remote_endpoint);

    // 2.1 设置rpc协议头部 2.2 设置rpc_meta控制信息 2.3 将request序列化
    crt->SetSequenceId(GenerateSequenceId());
    WriteBufferPtr writebuf_ptr;
    
    RpcHeader header;
    int header_size = sizeof(header);
    int pos = writebuf_ptr->Reserve(header_size);

    RpcMeta meta;
    meta.set_type(RpcMeta::REQUEST); // 设置为request类型
    meta.set_sequence_id(crt->GetSequenceId()); // 设置本次request id
    meta.set_service(crt->GetServiceName());
    meta.set_method(crt->GetMethodName());

    if(!meta.SerializeToZeroCopyStream(writebuf_ptr.get()))
    {
        LOG(ERROR, "CallMethod(): %s: serialize rpc meta failed", EndPointToString(crt->GetRemoteEndPoint()).c_str());
        crt->Done("serialized rpc meta data failed", true);
        return;
    }
    
    int meta_size = writebuf_ptr->ByteCount() - pos - header_size; // meta size
    
    if(!request->SerializeToZeroCopyStream(writebuf_ptr.get()))
    {
        LOG(ERROR, "CallMethod(): %s: serialize request failed", EndPointToString(crt->GetRemoteEndPoint()).c_str());
        crt->Done("serialized request data failed", true);
        return;
    }
    int data_size = writebuf_ptr->ByteCount() - pos - header_size - meta_size;

    header.meta_size = meta_size;
    header.data_size = data_size;
    header.message_size = meta_size + data_size;

    // 插入header
    writebuf_ptr->SetData(pos, reinterpret_cast<char*>(&header), header_size);
    ReadBufferPtr readbuf_ptr;
    writebuf_ptr->SwapOut(readbuf_ptr.get());

    crt->SetSendMessage(readbuf_ptr);
    // 3. 设置回调函数 回调函数中将cntl的收到的数据反序列化为response
    crt->PushDoneCallBack(std::bind(&RpcClient::DoneCallBack, shared_from_this(), response, std::placeholders::_1));

    // 4. 调用stream将数据发送给server端
    stream_ptr->CallMethod(crt->shared_from_this());
}


RpcClientStreamPtr RpcClient::FindOrCreateStream(const tcp::endpoint& endpoint)
{
    // Todo 多线程访问需要加锁
    if(_stream_map.count(endpoint))
    {
        return _stream_map[endpoint];
    }
    RpcClientStreamPtr ptr = std::make_shared<RpcClientStream>(_work_thread_group->GetService(), endpoint);
    _stream_map[endpoint] = ptr;
    // 建立连接
    ptr->AsyncConnect();
    return ptr;
}

uint64_t RpcClient::GenerateSequenceId()
{
    ++_next_request_id;
    return _next_request_id;
}

// 接收到回复反序列化response
void RpcClient::DoneCallBack(google::protobuf::Message* response, const RpcControllerPtr& crt)
{
    // 收到消息后反序列化response
    if(!crt->Failed())
    {
        CHECK(response);
        ReadBufferPtr& response_buf = crt->GetReceiveMessage();
        CHECK(response_buf.get());
        if(!response->ParseFromZeroCopyStream(response_buf.get()))
        {
            LOG(ERROR, "DoneCallBack(): %s: parse response message failed", EndPointToString(crt->GetRemoteEndPoint()).c_str());
            crt->SetFailed("parse response message failed");
        }
        else
        {
            LOG(DEBUG, "DoneCallBack(): %s: parse response message success", EndPointToString(crt->GetRemoteEndPoint()).c_str());
        }
    }
}

}