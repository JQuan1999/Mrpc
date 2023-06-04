#ifndef _MRPC_SERVER_STREAM_
#define _MRPC_SERVER_STREAM_

#include<memory>
#include<string>
#include<atomic>

#include<mrpc/server/rpc_request.h>
#include<mrpc/common/end_point.h>
#include<mrpc/common/rpc_byte_stream.h>
#include<mrpc/proto/rpc_header.h>

namespace mrpc
{
class RpcServerStream;
typedef std::shared_ptr<RpcServerStream> RpcServerStreamPtr;

class RpcServerStream: public RpcByteStream
{
public:
    typedef std::function<void(const RpcServerStreamPtr&, RpcRequest)> ReceiveCallBack;
    typedef std::function<void(const RpcServerStreamPtr&)> CloseCallback;
    RpcServerStream(IoContext& ioc, const tcp::endpoint& endpoint);

    ~RpcServerStream();

    void StartReceive();

    void OnReadHeader(const boost::system::error_code& ec, size_t bytes);

    void OnReadBody(const boost::system::error_code& ec, size_t bytes);

    void StartSend();

    void OnWriteHeader(const boost::system::error_code& ec, size_t bytes);

    void OnWriteBody(const boost::system::error_code& ec, size_t bytes);

    void ClearReceiveEnv();

    void NewBuffer();

    void SetReceiveCallBack(const ReceiveCallBack& callback);

    void SetCloseCallback(const CloseCallback& callback);

private:
    std::atomic<int> _receive_token;
    int _receive_free;
    int _receive_lock;
    
    int32_t _receive_bytes;
    ::mrpc::RpcHeader _header;
    Buffer _receive_data;
    ReadBufferPtr _readbuf_ptr;
    
    std::atomic<int> _send_token;
    int _send_free;
    int _send_lock;
    int32_t _send_bytes;

    ReceiveCallBack _receive_callback;
    CloseCallback _close_callback;
};
}


#endif