#ifndef _MRPC_BYTE_STREAM_H_
#define _MRPC_BYTE_STREAM_H_

#include<boost/asio.hpp>
#include<atomic>

#include<mrpc/common/logger.h>
#include<mrpc/common/end_point.h>
#define REVEIVE_FACTOR_SIZE 1
#define MAX_REVEIVE_FACTOR_SIZE 10
#define SEND_FACTOR_SIZE 1
#define MAX_SEND_FACTOR_SIZE 10

namespace mrpc{

class RpcByteStream;
typedef std::shared_ptr<RpcByteStream> RpcByteStreamPtr;

class RpcByteStream: public std::enable_shared_from_this<RpcByteStream>{
public:
    RpcByteStream(IoContext& ioc, const tcp::endpoint& endpoint)
        : _ioc(ioc)
        , _socket(_ioc)
        , _remote_endpoint(endpoint)
        , _local_endpoint(endpoint)
        , _receiving(false)
        , _sending(false)
        , _receive_factor_size(REVEIVE_FACTOR_SIZE)
        , _send_factor_size(SEND_FACTOR_SIZE)
        , _status(SOCKET_CLOSED)
    {

    }

    virtual ~RpcByteStream()
    {
        _socket.close();
    }

    void Close(const std::string msg)
    {
        if(_status.load() == SOCKET_CLOSED)
        {
            return;
        }
        else
        {
            LOG(INFO, "close(): remote: [%s] connection closed: %s", EndPointToString(GetRemote()).c_str(), msg.c_str());
            _status.store(SOCKET_CLOSED);
            _socket.close();
            OnClose(msg);
        }
    }

    bool IsClosed()
    {
        if(_status.load() == SOCKET_CLOSED)
        {
            return true;
        }else
        {
            return false;
        }
    }

    bool IsConnected()
    {
        if(_status.load() == SOCKET_CONNECTED)
        {
            return true;
        }else
        {
            return false;
        }
    }

    // uesd by server
    void SetConnected()
    {
        // Todo 设置no_delay
        _status.store(SOCKET_CONNECTED);
        StartReceive();
        StartSend();
    }

    // used by client
    void AsyncConnect()
    {
        _status.store(SOCKET_CONNECTING);
        _socket.async_connect(_remote_endpoint, std::bind(&RpcByteStream::OnConnect, shared_from_this(), std::placeholders::_1));
        // Todo 添加定时器
    }

    // common
    tcp::socket& GetSocket()
    {
        return _socket;
    }

    void UpdateRemote()
    {
        _remote_endpoint = _socket.remote_endpoint();
    }

    const tcp::endpoint& GetRemote()
    {
        return _remote_endpoint;
    }

    void UpdateLocal()
    {
        _local_endpoint = _socket.local_endpoint();
    }
    
    virtual void StartReceive() = 0;
    virtual void StartSend() = 0;

protected:

    bool TrySend()
    {
        std::lock_guard<std::mutex> lock(_sending_mutex);
        if(_sending.load() == true)
        {
            return false;
        }
        else
        {
            _sending.store(true);
            return true;
        }
    }

    void FreeSendingFlag()
    {
        std::lock_guard<std::mutex> lock(_sending_mutex);
        if(_sending.load() == false)
        {
            LOG(FATAL, "when FreeSendingFlag is called sending must be true");
        }
        else
        {
            _sending.store(false);
        }
    }
    
    bool TryReceive()
    {
        std::lock_guard<std::mutex> lock(_receiving_mutex);
        if(_receiving.load() == true)
        {
            return false;
        }
        else
        {
            _receiving.store(true);
            return true;
        }
    }

    void FreeReceivingFlag()
    {
        std::lock_guard<std::mutex> lock(_receiving_mutex);
        if(_receiving.load() == false)
        {
            LOG(FATAL, "when FreeSendingFlag is called sending must be true");
        }
        else
        {
            _receiving.store(false);
        }
    }

    // 由子类实现
    virtual void OnReadHeader(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnReadBody(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnWrite(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnClose(std::string reason) = 0;

    // 异步读数据
    void AsyncReadHeader(char* data, size_t size)
    {
        boost::asio::async_read(_socket, boost::asio::buffer(data, size), 
                                std::bind(&RpcByteStream::OnReadHeader, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    }

    void AsyncReadBody(char* data, size_t size)
    {
        boost::asio::async_read(_socket, boost::asio::buffer(data, size), 
                                std::bind(&RpcByteStream::OnReadBody, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    }

    void AsyncWrite(char* data, size_t size)
    {
        boost::asio::async_write(_socket, boost::asio::buffer(data, size), 
                                std::bind(&RpcByteStream::OnWrite, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    }

private:

    // call back of AsyncConnect()
    void OnConnect(const boost::system::error_code& ec)
    {
        if(ec){
            LOG(ERROR, "OnConnect(): connect erorr: %s: %s", EndPointToString(_remote_endpoint), ec.message().c_str());
            Close("connect erorr " + ec.message());
        }else{
            LOG(INFO, "OnConnect(): connect success from %s", EndPointToString(_remote_endpoint).c_str());
            _status.store(SOCKET_CONNECTED);
            StartReceive();
            StartSend();
        }
    }

protected:
    std::mutex _sending_mutex;
    std::atomic<bool> _sending;

    std::atomic<bool> _receiving;
    std::mutex _receiving_mutex;

    tcp::endpoint _local_endpoint;
    tcp::endpoint _remote_endpoint;
    IoContext& _ioc;
    int _receive_factor_size;
    int _send_factor_size;

private:
    enum SOCKET_STATUS{
        SOCKET_INIT = 0,
        SOCKET_CONNECTING = 1,
        SOCKET_CONNECTED = 2,
        SOCKET_CLOSED = 3
    };
    std::atomic<SOCKET_STATUS> _status;
    tcp::socket _socket;
};

}

#endif