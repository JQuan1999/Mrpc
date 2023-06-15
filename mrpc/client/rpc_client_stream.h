#ifndef _MRPC_CLIENT_STREAM_H
#define _MRPC_CLIENT_STREAM_H

#include<memory>
#include<map>
#include<atomic>
#include<mutex>

#include<mrpc/proto/rpc_meta.pb.h>
#include<mrpc/common/buffer.h>
#include<mrpc/common/rpc_controller.h>
#include<mrpc/common/end_point.h>
#include<mrpc/common/rpc_byte_stream.h>

namespace mrpc{

class RpcClientStream;
typedef std::shared_ptr<RpcClientStream> RpcClientStreamPtr;

class RpcClientStream: public RpcByteStream
{
public:
    RpcClientStream(IoContext& ioc, const tcp::endpoint& endpoint);

    ~RpcClientStream();

    void CallMethod(const RpcControllerPtr& crt);

private:

    void PutItem(const RpcControllerPtr& crt);

    bool GetItem();

    bool IsDone();

    void AddRequest(const RpcControllerPtr& crt);

    void EraseRequest(int sequence_id);

    virtual void StartSend();

    virtual void OnWrite(const boost::system::error_code& ec, size_t bytes);

    virtual void OnClose(std::string reason);

    virtual void StartReceive();

    virtual void OnReadHeader(const boost::system::error_code& ec, size_t bytes);

    virtual void OnReadBody(const boost::system::error_code& ec, size_t bytes);

    virtual void OnReceived();

    void NewReceiveBuffer();

    // 重置接收的临时变量
    void ClearReceiveEnv();

private:
    int _receive_bytes;
    RpcHeader _header;
    Buffer _receive_data;
    ReadBufferPtr _readbuf_ptr;

    int _send_bytes;
    const void* _send_data;
    ReadBufferPtr _sendbuf_ptr; // 当前正发送的buf
    RpcControllerPtr _send_crt; // 当前正发送的crt
    std::deque<RpcControllerPtr> _send_buf_queue;

    std::mutex _send_mutex;
    std::mutex _controller_map_mutex;
    std::map<uint64_t, RpcControllerPtr> _controller_map; // sequence_id -> controller
};
}

#endif