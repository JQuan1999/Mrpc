#ifndef _MRPC_CONTROLLER_H_
#define _MRPC_CONTROLLER_H_
#include<google/protobuf/service.h>
#include<google/protobuf/message.h>
#include<string.h>
#include<stack>
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<boost/asio.hpp>

#include<mrpc/common/logger.h>
#include<mrpc/common/buffer.h>
#include<mrpc/common/end_point.h>
#include<mrpc/server/rpc_server_stream.h>

namespace mrpc{

class RpcController;
typedef std::shared_ptr<RpcController> RpcControllerPtr;

class RpcController
        : public google::protobuf::RpcController
        , public std::enable_shared_from_this<RpcController>
{
public:
    typedef std::function<void(RpcControllerPtr)> callback;
    RpcController();

    ~RpcController();
    // Client-side methods ---------------------------------------------
    // These calls may be made from the client side only.  Their results
    // are undefined on the server side (may crash).

    // Resets the RpcController to its initial state so that it may be reused in
    // a new call.  Must not be called while an RPC is in progress.
    virtual void Reset();

    // After a call has finished, returns true if the call failed.  The possible
    // reasons for failure depend on the RPC implementation.  Failed() must not
    // be called before a call has finished.  If Failed() returns true, the
    // contents of the response message are undefined.
    virtual bool Failed() const;

    // If Failed() is true, returns a human-readable description of the error.
    virtual std::string ErrorText() const;

    // Advises the RPC system that the caller desires that the RPC call be
    // canceled.  The RPC system may cancel it immediately, may wait awhile and
    // then cancel it, or may not even cancel the call at all.  If the call is
    // canceled, the "done" callback will still be called and the RpcController
    // will indicate that the call failed at that time.
    virtual void StartCancel();

    // Server-side methods ---------------------------------------------
    // These calls may be made from the server side only.  Their results
    // are undefined on the client side (may crash).

    // Causes Failed() to return true on the client side.  "reason" will be
    // incorporated into the message returned by ErrorText().  If you find
    // you need to return machine-readable information about failures, you
    // should incorporate it into your response protocol buffer and should
    // NOT call SetFailed().
    virtual void SetFailed(const std::string& reason);

    // If true, indicates that the client canceled the RPC, so the server may
    // as well give up on replying to it.  The server should still call the
    // final "done" callback.
    virtual bool IsCanceled() const;

    // Asks that the given callback be called when the RPC is canceled.  The
    // callback will always be called exactly once.  If the RPC completes without
    // being canceled, the callback will be called after completion.  If the RPC
    // has already been canceled when NotifyOnCancel() is called, the callback
    // will be called immediately.
    //
    // NotifyOnCancel() must be called no more than once per request.
    virtual void NotifyOnCancel(google::protobuf::Closure* callback);

    // client-side method
    void SetMethodName(const std::string method_name);

    const std::string& GetMethodName();
    
    void SetServiceName(const std::string service_name);
    
    const std::string& GetServiceName();
    
    void Done(std::string reason, bool failed);
    
    void PushDoneCallBack(callback func);
    
    void SetSync();
    
    bool IsSync();
    
    void StartTime();
    
    void Wait(); // 阻塞
    
    void Signal(); // 唤醒回调函数中执行
    
    void SetRemoteEndPoint(const tcp::endpoint&);

    void SetSendMessage(const ReadBufferPtr& sendbuf);

    ReadBufferPtr& GetSendMessage();
    
    const tcp::endpoint GetRemoteEndPoint();

    void SetReceiveMessage(ReadBufferPtr sendbuf);

    ReadBufferPtr& GetReceiveMessage();

    void SetSuccess();

    // server side
    void SetSeverStream(const RpcServerStreamPtr& server_stream);

    RpcServerStreamPtr GetSeverStream();

    void SetRequest(google::protobuf::Message* request);

    google::protobuf::Message* GetRequest();

    void SetResponse(google::protobuf::Message* response);

    google::protobuf::Message* GetResponse();

    // common
    bool IsDone();

    void SetSequenceId(uint64_t);
    
    uint64_t GetSequenceId();

private:
    // client
    std::string _reason;
    bool _failed;
    std::atomic<bool> _done; // 是否完成
    bool _is_sync; // 是否同步
    bool _is_signal; // 是否唤醒
    std::stack<callback> _callback_queue; // 回调函数队列
    std::mutex _mutex;
    std::condition_variable _cond;
    ReadBufferPtr _send_buf;
    ReadBufferPtr _receive_buf;

    // server
    RpcServerStreamPtr _server_stream;
    google::protobuf::Message* _response;
    google::protobuf::Message* _request;

    // common
    uint64_t _sequence_id;
    boost::asio::ip::tcp::endpoint _remote_endpoint;
    boost::asio::ip::tcp::endpoint _local_endpoint;
    std::string _method_name;
    std::string _service_name;
};
}

#endif