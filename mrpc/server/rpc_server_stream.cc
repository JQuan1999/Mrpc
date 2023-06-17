#include<mrpc/server/rpc_server_stream.h>
#include<mrpc/server/rpc_request.h>

namespace mrpc
{

RpcServerStream::RpcServerStream(IoContext& ioc, const tcp::endpoint& endpoint)
    : RpcByteStream(ioc, endpoint)
    , _receive_bytes(0)
    , _receive_data()
    , _readbuf_ptr(new ReadBuffer())
    , _send_bytes(0)
    , _send_data(nullptr)
{

}

RpcServerStream::~RpcServerStream()
{
    LOG(DEBUG, "in ~RpcServerStream()");
    Close("rpc server stream destructed");
}

void RpcServerStream::SendResponse(ReadBufferPtr readbuf)
{
    if(IsClosed())
    {
        return;
    }
    PutItem(readbuf);
    StartSend();
}

void RpcServerStream::StartSend()
{
    if(!IsConnected())
    {
        return;
    }
    if(TrySend())
    {
        ClearSendEnv();
        if(!GetItem())
        {
            LOG(DEBUG, "StartSend(): remote: [%s] the send buf queue is empty", EndPointToString(_remote_endpoint).c_str());
            FreeSendingFlag();
            return;
        }
        else
        {
            if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes) || _send_bytes == 0)
            {
                LOG(DEBUG, "StartSend(): remote: [%s] sendbuf is empty", EndPointToString(_remote_endpoint).c_str());
                FreeSendingFlag();
            }
            else
            {
                AsyncWrite((char*)_send_data, _send_bytes);
            }
        }
    }
}

void RpcServerStream::OnWrite(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
        LOG(ERROR, "write to:%s error msg: %s", EndPointToString(_remote_endpoint).c_str(), ec.message().c_str());
        Close("write error");
        return;
    }else
    {
        if(bytes < _send_bytes)
        {
            _send_data += bytes;
            _send_bytes -= bytes;
            AsyncWrite((char*)_send_data, _send_bytes);
        }
        else
        {
            LOG(DEBUG, "success write %d bytes data to: %s", bytes, EndPointToString(_remote_endpoint).c_str());
            if(!_sendbuf_ptr->Next(&_send_data, &_send_bytes))
            {
                FreeSendingFlag();
                StartSend();
            }
            else
            {
                AsyncWrite((char*)_send_data, _send_bytes); // 还有数据继续发送
            }
        }
    }
}

void RpcServerStream::OnClose(std::string)
{
    _close_callback(std::dynamic_pointer_cast<RpcServerStream>(shared_from_this()));
}

void RpcServerStream::PutItem(ReadBufferPtr& readbuf)
{
    std::lock_guard<std::mutex> lock(_send_mutex);
    _send_buf_queue.push_back(readbuf);
}

bool RpcServerStream::GetItem()
{
    std::lock_guard<std::mutex> lock(_send_mutex);
    if(_send_buf_queue.empty())
    {
        return false;
    }
    else
    {
        _sendbuf_ptr = _send_buf_queue.front();
        _send_buf_queue.pop_front();
        return true;
    }
}

void RpcServerStream::StartReceive()
{
    // 开始接收数据
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

void RpcServerStream::OnReadHeader(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
        if(ec == boost::asio::error::eof)
        {
            LOG(ERROR, "OnReadHeader(): server: %s has closed connection error msg: %s", 
                EndPointToString(_remote_endpoint).c_str(), ec.message().c_str());
            Close("client closed");
        }else{
            LOG(ERROR, "OnReadHeader(): server: %s read header error error msg: %s", 
                EndPointToString(_remote_endpoint).c_str() ,ec.message().c_str());
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

void RpcServerStream::OnReadBody(const boost::system::error_code& ec, size_t bytes)
{
    if(ec)
    {
        if(ec == boost::asio::error::eof)
        {
            LOG(ERROR, "OnReadHeader(): server: %s has closed connection error msg: %s", EndPointToString(_remote_endpoint), ec.message().c_str());
            Close("client closed");
        }else{
            LOG(ERROR, "OnReadHeader(): server: %s read header error error msg: %s", EndPointToString(_remote_endpoint),ec.message().c_str());
            Close("read error");
        }
        return;
    }
    else
    {
        _receive_bytes += bytes;
        _receive_data.Forward(bytes);
        // change write to read
        _receive_data.SetCapacity(_receive_data.GetSize());
        _receive_data.SetSize(0);
        _readbuf_ptr->Append(_receive_data);
        if(_receive_bytes == _header.message_size)
        {
            // 收到一条完整的request
            RpcRequest request(_header, _readbuf_ptr);
            FreeReceivingFlag();
            StartReceive();
            // 开始解析request
            // dynamic_pointer_cast将指向基类的智能指针转换为指向派生类的智能指针
            _receive_callback(std::dynamic_pointer_cast<RpcServerStream>(shared_from_this()), request);
        }
        else
        {
            // 还未接收到完整的request继续进行接收
            _receive_factor_size = std::min(_receive_factor_size+1, MAX_REVEIVE_FACTOR_SIZE);
            NewBuffer();
            int res_data_size = _header.message_size - _receive_bytes; // 剩余数据大小
            int read_size = std::min(res_data_size, _receive_data.GetCapacity()); // 可以读取的数据大小
            AsyncReadBody(_receive_data.GetHeader(), read_size); // 继续进行读数据
        }
    }
}

void RpcServerStream::ClearReceiveEnv()
{
    _receive_factor_size = REVEIVE_FACTOR_SIZE;
    _receive_bytes = 0;
    _readbuf_ptr.reset(new ReadBuffer());
    NewBuffer();
}

void RpcServerStream::ClearSendEnv()
{
    _send_bytes = 0;
    _sendbuf_ptr.reset();
}

void RpcServerStream::NewBuffer()
{
    _receive_data = Buffer(_receive_factor_size);
}

void RpcServerStream::SetReceiveCallBack(const ReceiveCallBack& callback)
{
    _receive_callback = callback;
}

void RpcServerStream::SetCloseCallback(const CloseCallback& callback)
{
    _close_callback = callback;
}

}