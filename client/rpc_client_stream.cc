#include<mrpc/client/rpc_client_stream.h>

namespace mrpc
{
RpcClientStream::RpcClientStream(IoContext& ioc, const tcp::endpoint& endpoint)
    : RpcByteStream(ioc, endpoint)
    , _receive_bytes(0)
    , _receive_data()
    , _send_data(nullptr)
{

}

RpcClientStream::~RpcClientStream()
{
    // Need lock?
    for(auto iter = _controller_map.begin(); iter != _controller_map.end(); iter++)
    {
        iter->second->Done("RpcClientStream destructed", true);
    }
}


void RpcClientStream::CallMethod(const RpcControllerPtr& crt)
{
    if(!IsConnected())
    {
        LOG(ERROR, "CallMethod(): socket is not connected: %s", EndPointToString(_remote_endpoint).c_str());
        crt->Done("socket is not connected", true);
        return;
    }
    AddRequest(crt);
    PutItem(crt);
    StartSend();
}

void RpcClientStream::StartSend()
{
    if(TrySend())
    {
        if(!GetItem())
        {
            LOG(DEBUG, "StartSend(): the send buf queue is empty");
            FreeSendingFlag();
            return;
        }
        if(IsDone())
        {
            LOG(DEBUG, "StartSend(): the rpc request has been done maybe timeout: %s", 
                        EndPointToString(_remote_endpoint).c_str());
            FreeSendingFlag();
            return;
        }
        if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes))
        {
            LOG(DEBUG, "StartSend(): the sendbuf is empty or get data failed: %s",
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
        _send_crt->Done("write erorr", true);
        EraseRequest(_send_crt->GetSequenceId());
        // 关闭连接
        Close(ec.message());
        FreeSendingFlag();
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
        if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes)) // _send_buf为空数据已发送完
        {
            LOG(DEBUG, "send data to %s succeed", EndPointToString(_remote_endpoint).c_str());
            FreeSendingFlag(); // 在回调函数中恢复_sending
            StartSend(); // 继续尝试发送队列剩余数据
        }
        else
        {
            AsyncWrite((char*)_send_data, _send_bytes); // 数据未发送完继续发送 _send_buf有多个数据块
        }
    }
}

void RpcClientStream::PutItem(const RpcControllerPtr& crt)
{
    std::lock_guard<std::mutex> lock(_send_mutex);
    _send_buf_queue.push_back(crt);
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
        _send_crt = _send_buf_queue.front();
        _sendbuf_ptr = _send_crt->GetSendMessage();
        _send_buf_queue.pop_front();
        return true;
    }
}


void RpcClientStream::StartReceive()
{
    if(!IsConnected())
    {
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
        LOG(ERROR, "OnReadHeader(): read proto header error, error msg: ", ec.message());
        Close("read error");
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
            LOG(ERROR, "OnReadBody(): client has closed connection, error msg: ", ec.message());
        }else{
            LOG(ERROR, "OnReadBody(): read request body error, error msg: ", ec.message());
        }
        Close("read error");
        return;
    }
    else
    {
        _receive_bytes += bytes;
        _receive_data.Forward(bytes);
        _readbuf_ptr->Append(_receive_data);
        if(_receive_bytes == _header.message_size)
        {
            // 收到完整的消息
            OnReceived();
            FreeReceivingFlag();
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

void RpcClientStream::OnReceived()
{
    RpcMeta meta;
    ReadBufferPtr meta_buf = _readbuf_ptr->Split(_header.meta_size);
    ReadBufferPtr message_buf = _readbuf_ptr;
    if(!meta.ParseFromZeroCopyStream(meta_buf.get()))
    {
        LOG(ERROR, "OnReceived(): parse metabuf erorr");
        return;
    }
    // 检查是否为request
    RpcMeta_Type type = meta.type();
    if(type != RpcMeta_Type_RESPONSE)
    {
        LOG(ERROR, "OnReceived(): the received type is not response");
        return;
    }
    uint64_t sequence_id = meta.sequence_id();
    RpcControllerPtr contro_ptr;
    if(_controller_map.find(sequence_id) == _controller_map.end())
    {
        LOG(ERROR, "OnReceived(): sequence_id:%d controller is not existed", sequence_id);
        return;
    }
    else
    {
        contro_ptr = _controller_map[sequence_id];
        EraseRequest(sequence_id);
    }
    // 检查是否已经超时
    if(contro_ptr->IsDone())
    {
        LOG(INFO, "OnReceived(): %s {%lu}: request has already done(maybe timeout)", EndPointToString(_remote_endpoint).c_str(), sequence_id);
        return;
    }

    // 检查是否失败
    if(meta.failed())
    {
        if(meta.has_reason())
        {
            contro_ptr->Done(meta.reason(), true);
        }
        else
        {
            contro_ptr->Done("request maybe failed but reason is not set", true);
        }
    }
    else
    {
        contro_ptr->SetReceiveMessage(message_buf);
        contro_ptr->Done("success", false);
    }
}

void RpcClientStream::ClearReceiveEnv()
{
    _receive_factor_size = REVEIVE_FACTOR_SIZE;
    _receive_bytes = 0;
    _readbuf_ptr.reset();
    NewReceiveBuffer();
}

void RpcClientStream::NewReceiveBuffer()
{
    _receive_data = Buffer(_receive_factor_size);
}


bool RpcClientStream::IsDone()
{
    return _send_crt->IsDone();
}


void RpcClientStream::AddRequest(const RpcControllerPtr& crt)
{
    uint64_t _id = crt->GetSequenceId();
    // Todo check existed
    std::lock_guard<std::mutex> lock(_controller_map_mutex);
    _controller_map[_id] = crt;
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