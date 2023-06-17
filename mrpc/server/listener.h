#ifndef _MRPC_LISTENER_H_
#define _MRPC_LISTENER_H_
#include<boost/asio.hpp>
#include<mutex>
#include<atomic>
#include<memory>
#include<functional>

#include<mrpc/common/logger.h>
#include<mrpc/common/end_point.h>
#include<mrpc/server/rpc_server_stream.h>

namespace mrpc{

class Listener;
typedef std::shared_ptr<Listener> ListenerPtr;

class Listener: public std::enable_shared_from_this<Listener>
{
public:
    typedef std::function<void(const RpcServerStreamPtr&)> callback;
    explicit Listener(IoContext& ioc, const tcp::endpoint& endpoint)
        : _ioc(ioc)
        , _endpoint(endpoint)
        , _acceptor(_ioc)
        , _accept_callback(nullptr)
        , _create_callback(nullptr)
    {
        _is_closed.store(true);
    }

    ~Listener()
    {
        Stop();
    }

    void SetAcceptCallback(callback accpet_callback)
    {
        _accept_callback = accpet_callback;
    }

    void SetCreateCallback(callback create_callback)
    {
        _create_callback = create_callback;
    }

    bool StartListen()
    {
        boost::system::error_code ec;
        _acceptor.open(_endpoint.protocol(), ec); // open acceptor using the specified protocol
        if(ec)
        {
            LOG(ERROR, "StartListen(): open acceptor failed: %s: %s", EndPointToString(_endpoint).c_str(), ec.message().c_str());
            return false;
        }
        
        _acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if(ec){
            LOG(ERROR, "StartListen(): set acceptor option failed: %s: %s", EndPointToString(_endpoint).c_str(), ec.message().c_str());
            return false;
        }

        _acceptor.bind(_endpoint, ec);
        if(ec){
            LOG(ERROR, "StartListen(): bind acceptor failed: %s: %s", EndPointToString(_endpoint).c_str(), ec.message().c_str());
            return false;
        }

        _acceptor.listen(4096, ec);
        if(ec){
            LOG(ERROR, "StartListen(): listen acceptor failed: %s: %s", EndPointToString(_endpoint).c_str(), ec.message().c_str());
            return false;
        }
        _is_closed.store(false); // 将_is_closed设为false 表示已经打开开始接受连接
        LOG(INFO, "StartListen(): listen succeed: %s", EndPointToString(_endpoint).c_str());
        AsyncAccpet();
        return true;
    }

    void Stop()
    {
        if(_is_closed.load() == true)
        {
            return;
        }
        _is_closed.store(true);
        boost::system::error_code ec;
        _acceptor.cancel(ec);
        _acceptor.close(ec);
        LOG(INFO, "Stop(): listener stoped: %s", EndPointToString(_endpoint).c_str());
    }

private:
    void AsyncAccpet()
    {
        RpcServerStreamPtr stream = std::make_shared<RpcServerStream>(_ioc, _endpoint);
        if(_create_callback)
        {
            _create_callback(stream);
        }
        _acceptor.async_accept(stream->GetSocket(), std::bind(&Listener::OnAccept, shared_from_this(), 
                                stream, std::placeholders::_1));
    }

    // const stream&
    void OnAccept(RpcServerStreamPtr stream, const boost::system::error_code& ec)
    {
        if(_is_closed.load() == true)
        {
            return;
        }

        if(ec)
        {
            LOG(ERROR, "OnAccept(): async accpet error: %s", EndPointToString(_endpoint).c_str());
            Stop();
            return;
        }
        else
        {
            if(_accept_callback)
            {
                _accept_callback(stream); // 调用server::OnAccept
            }
            AsyncAccpet(); // 继续接收新连接
        }
    }

private:
    std::atomic<bool> _is_closed;
    IoContext& _ioc;
    callback _create_callback;
    callback _accept_callback;
    tcp::acceptor _acceptor;
    tcp::endpoint _endpoint;
};

}

#endif