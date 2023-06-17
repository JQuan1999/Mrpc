#ifndef _MRPC_SERVER_STREAM_
#define _MRPC_SERVER_STREAM_

#include<memory>
#include<string>
#include<atomic>
#include<deque>

#include<mrpc/common/end_point.h>
#include<mrpc/common/rpc_byte_stream.h>
#include<mrpc/common/buffer.h>
#include<mrpc/proto/rpc_header.h>

namespace mrpc
{
class RpcRequest;
class RpcServerStream;
typedef std::shared_ptr<RpcServerStream> RpcServerStreamPtr;

class RpcServerStream: public RpcByteStream
{
public:
    typedef std::function<void(const RpcServerStreamPtr&, RpcRequest)> ReceiveCallBack;
    typedef std::function<void(const RpcServerStreamPtr&)> CloseCallback;

    RpcServerStream(IoContext& ioc, const tcp::endpoint& endpoint);

    ~RpcServerStream();

    void SendResponse(ReadBufferPtr readbuf);

    void PutItem(ReadBufferPtr& readbuf);

    bool GetItem();

    virtual void StartSend();

    virtual void OnClose(std::string);
    
    virtual void OnWrite(const boost::system::error_code& ec, size_t bytes);

    virtual void StartReceive();

    virtual void OnReadHeader(const boost::system::error_code& ec, size_t bytes);

    virtual void OnReadBody(const boost::system::error_code& ec, size_t bytes);    

    void ClearReceiveEnv();

    void ClearSendEnv();

    void NewBuffer();

    void SetReceiveCallBack(const ReceiveCallBack& callback);

    void SetCloseCallback(const CloseCallback& callback);

private: 
    int32_t _receive_bytes;
    ::mrpc::RpcHeader _header;
    Buffer _receive_data;
    ReadBufferPtr _readbuf_ptr;
    
    int _send_bytes;
    const void* _send_data;
    ReadBufferPtr _sendbuf_ptr;
    std::deque<ReadBufferPtr> _send_buf_queue;
    std::mutex _send_mutex;

    ReceiveCallBack _receive_callback;
    CloseCallback _close_callback;
};
}


#endif