#include<mrpc/server/rpc_server_stream.h>

namespace mrpc
{

RpcServerStream::RpcServerStream(IoContext& ioc, const tcp::endpoint& endpoint)
    : RpcByteStream(ioc, endpoint)
    , _receive_free(0)
    , _receive_lock(1)
    , _receive_token(0)
    , _send_free(0)
    , _send_lock(1)
    , _send_token(0)
    , _receive_bytes(0)
    , _receive_data(_receive_factor_size)
{

}

RpcServerStream::~RpcServerStream()
{
    Close("rpc server stream destructed");
}


void RpcServerStream::StartReceive()
{
    // 开始接收数据
    if(_send_token.compare_exchange_strong(_send_free, _send_lock))
    {
        ClearReceiveEnv();
        AsyncReadHeader(reinterpret_cast<char*>(&_header), sizeof(_header));
    }
    else
    {
        _send_free = 0;
    }
}

void RpcServerStream::OnReadHeader(const boost::system::error_code& ec, size_t bytes)
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

void RpcServerStream::OnReadBody(const boost::system::error_code& ec, size_t bytes)
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
            // 收到一条完整的request
            RpcRequest _request(_header, _readbuf_ptr);
            _receive_token.compare_exchange_strong(_receive_lock, _receive_free);
            StartReceive();
            // 开始解析request
            // dynamic_pointer_cast将指向基类的智能指针转换为指向派生类的智能指针
            _receive_callback(std::dynamic_pointer_cast<RpcServerStream>(shared_from_this()), _request);
            
        }else
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
    _readbuf_ptr.reset();
    NewBuffer();
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