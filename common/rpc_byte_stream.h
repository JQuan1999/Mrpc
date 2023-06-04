#ifndef _MRPC_BYTE_STREAM_H_
#define _MRPC_BYTE_STREAM_H_

#include<boost/asio.hpp>
#include<atomic>

#include<mrpc/common/logger.h>
#include<mrpc/common/end_point.h>
#define REVEIVE_FACTOR_SIZE 3
#define MAX_REVEIVE_FACTOR_SIZE 10
#define SEND_FACTOR_SIZE 3
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
        , _receive_factor_size(REVEIVE_FACTOR_SIZE)
        , _send_factor_size(SEND_FACTOR_SIZE)
    {
        _status.store(SOCKET_CLOSED);
    }

    ~RpcByteStream()
    {
        _socket.close();
    }

    void Close(const std::string msg){
        if(_status.load() == SOCKET_CLOSED)
        {
            return;
        }
        else
        {
            _status.store(SOCKET_CLOSED);
            _socket.close();
            LOG(INFO, "close(): connection closed: %s", msg.c_str());
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

    bool IsConnected()
    {
        return _status.load() == SOCKET_CONNECTED;
    }

    // 由子类实现
    virtual void OnReadHeader(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnReadBody(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnWriteHeader(const boost::system::error_code& ec, size_t bytes) = 0;
    virtual void OnWriteBody(const boost::system::error_code& ec, size_t bytes) = 0;

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

    void AysncWriteHeader(char* data, size_t size)
    {
        boost::asio::async_write(_socket, boost::asio::buffer(data, size), 
                                std::bind(&RpcByteStream::OnWriteHeader, shared_from_this(), 
                                std::placeholders::_1, std::placeholders::_2));
    }

    void AysncWriteBody(char* data, size_t size)
    {
        boost::asio::async_write(_socket, boost::asio::buffer(data, size), 
                                std::bind(&RpcByteStream::OnWriteBody, shared_from_this(), 
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
            LOG(INFO, "OnConnect(): connect success from %s", EndPointToString(_remote_endpoint));
            _status.store(SOCKET_CONNECTED);
            StartReceive();
            StartSend();
        }
    }

protected:
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