#include<mrpc/client/rpc_client_stream.h>

namespace mrpc
{
RpcClientStream::RpcClientStream(IoContext& ioc, const tcp::endpoint& endpoint)
    : RpcByteStream(ioc, endpoint)
    , _receive_bytes(0)
    , _receive_data()
    , _readbuf_ptr(new ReadBuffer())
    , _send_bytes(0)
    , _send_data(nullptr)
    , _close_callback(nullptr)
{

}

RpcClientStream::~RpcClientStream()
{
    Close("rpc stream destructed");
}

void RpcClientStream::CallMethod(const RpcControllerPtr& cnt)
{
    if(!IsConnected())
    {
        LOG(ERROR, "CallMethod(): socket is not connected: %s", EndPointToString(_remote_endpoint).c_str());
        cnt->Done("socket is not connected", true);
        return;
    }
    AddRequest(cnt);
    PutItem(cnt);
    StartSend();
}

void RpcClientStream::SetCloseCallback(callback close_callback)
{
    _close_callback = close_callback;
}

void RpcClientStream::StartSend()
{
    if(TrySend())
    {
        ClearSendEnv();
        if(!GetItem())
        {
            FreeSendingFlag();
            return;
        }
        if(IsDone())
        {
            LOG(DEBUG, "StartSend(): remote: %s the rpc request has been done maybe timeout", 
                        EndPointToString(_remote_endpoint).c_str());
            FreeSendingFlag();
            return;
        }
        if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes))
        {
            LOG(DEBUG, "StartSend(): remote: %s the sendbuf is empty or get data failed",
                        EndPointToString(_remote_endpoint).c_str());
            FreeSendingFlag();
            return;
        }
        // 进行发送
        AsyncWrite((char*)_send_data, _send_bytes);
    }
}

void RpcClientStream::OnWrite(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
        LOG(ERROR, "OnWriteSome(): %s: write erorr: %s", 
            EndPointToString(_remote_endpoint).c_str(), ec.message().c_str());
        // 调用当前crt的done函数
        _send_cnt->Done("write erorr", true);
        EraseRequest(_send_cnt->GetSequenceId());
        // 关闭连接
        Close(ec.message());
        return;
    }
    if(bytes < _send_bytes)
    {
        _send_data += bytes;
        _send_bytes -= bytes;
        AsyncWrite((char*)_send_data, _send_bytes); // 当前数据块未发送完 继续发送剩余数据
        return;
    }
    else
    {
        LOG(DEBUG, "OnWrite(): success write %d bytes data to: %s", bytes, EndPointToString(_remote_endpoint).c_str());
        if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes)) // _send_buf为空数据已发送完
        {
            FreeSendingFlag(); // 在回调函数中恢复_sending
            StartSend(); // 继续尝试发送队列剩余数据
        }
        else
        {
            AsyncWrite((char*)_send_data, _send_bytes); // 数据未发送完继续发送 _send_buf有多个数据块
        }
    }
}

void RpcClientStream::OnClose(std::string reason)
{
    LOG(DEBUG, "OnClose(): remote [%s] realease all wait rpc controller", EndPointToString(_remote_endpoint).c_str());
    std::lock_guard<std::mutex> lock(_controller_map_mutex);
    for(auto& p: _controller_map)
    {
        p.second->Done(reason, false);
    }
    if(_close_callback)
    {
        _close_callback(std::dynamic_pointer_cast<RpcClientStream>(shared_from_this()));
    }
}

void RpcClientStream::PutItem(const RpcControllerPtr& cnt)
{
    std::lock_guard<std::mutex> lock(_send_mutex);
    _send_buf_queue.push_back(cnt);
}

bool RpcClientStream::GetItem()
{
    std::lock_guard<std::mutex> lock(_send_mutex);
    if(_send_buf_queue.empty())
    {
        return false;
    }
    else
    {
        _send_cnt = _send_buf_queue.front();
        _sendbuf_ptr = _send_cnt->GetSendMessage();
        _send_buf_queue.pop_front();
        return true;
    }
}


void RpcClientStream::StartReceive()
{
    if(!IsConnected())
    {
        LOG(ERROR, "StartReceive(): remote: %s is not connected", EndPointToString(_remote_endpoint).c_str());
        return;
    }
    if(TryReceive())
    {
        ClearReceiveEnv();
        AsyncReadHeader(reinterpret_cast<char*>(&_header), sizeof(_header));
    }
}

void RpcClientStream::OnReadHeader(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
         if(ec == boost::asio::error::eof)
        {
            LOG(ERROR, "OnReadHeader(): server: %s has closed connection error msg: %s", EndPointToString(_remote_endpoint).c_str(), ec.message().c_str());
            Close("client closed");
        }else{
            LOG(ERROR, "OnReadHeader(): server: %s read header error error msg: %s", EndPointToString(_remote_endpoint).c_str(),ec.message().c_str());
            Close("read error");
        }
        return;
    }
    else
    {
        int res_data_size = _header.message_size - _receive_bytes; // 剩余数据大小
        int read_size = std::min(_receive_data.GetSpace(), _header.message_size); // 可以读取的数据大小
        AsyncReadBody(_receive_data.GetHeader(), read_size);
    }
}

void RpcClientStream::OnReadBody(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
        if(ec == boost::asio::error::eof)
        {
            LOG(ERROR, "OnReadHeader(): server: %s has closed connection error msg: %s", EndPointToString(_remote_endpoint), ec.message().c_str());
            Close("client closed");
        }
        else
        {
            LOG(ERROR, "OnReadHeader(): server: %s read header error error msg: %s", EndPointToString(_remote_endpoint),ec.message().c_str());
            Close("read error");
        }
        return;
    }
    else
    {
        _receive_bytes += bytes;
        _receive_data.Forward(bytes);

        _receive_data.SetCapacity(_receive_data.GetSize());
        _receive_data.SetSize(0);
        _readbuf_ptr->Append(_receive_data);
        if(_receive_bytes == _header.message_size)
        {
            // 收到完整的消息
            OnReceived(_readbuf_ptr);
            FreeReceivingFlag();
            StartReceive();
        }
        else
        {
            // 还未接收到完整的request继续进行接收
            _receive_factor_size = std::min(_receive_factor_size+1, MAX_REVEIVE_FACTOR_SIZE);
            NewReceiveBuffer();
            int res_data_size = _header.message_size - _receive_bytes; // 剩余数据大小
            int read_size = std::min(res_data_size, _receive_data.GetCapacity()); // 可以读取的数据大小
            AsyncReadBody(_receive_data.GetHeader(), read_size); // 继续进行读数据
        }
    }
}

void RpcClientStream::OnReceived(ReadBufferPtr readbuf)
{
    RpcMeta meta;
    ReadBufferPtr meta_buf = readbuf->Split(_header.meta_size);
    ReadBufferPtr data_buf = readbuf;
    if(!meta.ParseFromZeroCopyStream(meta_buf.get()))
    {
        LOG(ERROR, "OnReceived(): remote: [%s] parse metabuf erorr", EndPointToString(_remote_endpoint).c_str());
        return;
    }
    // 检查是否为request
    RpcMeta_Type type = meta.type();
    if(type != RpcMeta_Type_RESPONSE)
    {
        LOG(ERROR, "OnReceived(): remote: [%s] the received type is not response", EndPointToString(_remote_endpoint).c_str());
        return;
    }
    uint64_t sequence_id = meta.sequence_id();
    if(sequence_id == 0) // 服务端解析meta出错 sequnce_id = 0
    {
        LOG(ERROR, "OnReceived(): remote: [%s] the sequence_id is zero maybe server parser meta data failed", 
            EndPointToString(_remote_endpoint).c_str());
        return;
    }

    // 找到sequnce_id对应的cnt
    RpcControllerPtr cnt;
    if(_controller_map.find(sequence_id) == _controller_map.end())
    {
        LOG(ERROR, "OnReceived(): remote: [%s] sequence_id:%lu controller is not existed may be timeout", 
            EndPointToString(_remote_endpoint).c_str(), sequence_id);
        return;
    }
    else
    {
        cnt = _controller_map[sequence_id];
        EraseRequest(sequence_id);
    }
    // 检查是否已经超时
    if(cnt->IsDone())
    {
        LOG(INFO, "OnReceived(): %s {%lu}: request has already done maybe timeout", EndPointToString(_remote_endpoint).c_str(), sequence_id);
        return;
    }

    // 检查是否失败
    if(meta.failed())
    {
        if(meta.has_reason())
        {
            cnt->SetRemoteReason(meta.reason());
            cnt->Done("", true);
        }
        else
        {
            cnt->Done("request maybe failed but reason is not set", true);
        }
    }

    // 反序列化response
    google::protobuf::Message* response = cnt->GetResponse();
    CHECK(response);
    if(!response->ParseFromZeroCopyStream(data_buf.get()))
    {
        LOG(ERROR, "OnReceived(): reomte: [%s] parse response message failed", EndPointToString(_remote_endpoint).c_str());
        cnt->Done("parse response message failed", true);
    }
    else
    {
        cnt->Done("callmethod success", false);
    }
}

void RpcClientStream::ClearReceiveEnv()
{
    _receive_factor_size = REVEIVE_FACTOR_SIZE;
    _receive_bytes = 0;
    _readbuf_ptr.reset(new ReadBuffer());
    NewReceiveBuffer();
}

void RpcClientStream::ClearSendEnv()
{
    _send_cnt.reset();
    _sendbuf_ptr.reset();
    _send_bytes = 0;
}

void RpcClientStream::NewReceiveBuffer()
{
    _receive_data = Buffer(_receive_factor_size);
}


bool RpcClientStream::IsDone()
{
    return _send_cnt->IsDone();
}


void RpcClientStream::AddRequest(const RpcControllerPtr& cnt)
{
    uint64_t _id = cnt->GetSequenceId();
    // Todo check existed
    std::lock_guard<std::mutex> lock(_controller_map_mutex);
    _controller_map[_id] = cnt;
}

void RpcClientStream::EraseRequest(int sequence_id)
{
    std::lock_guard<std::mutex> lock(_controller_map_mutex);
    if(_controller_map.count(sequence_id))
    {
        _controller_map.erase(sequence_id);
    }
}

}