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

    virtual void Reset();

    virtual bool Failed() const;

    // If Failed() is true, returns a human-readable description of the error.
    virtual std::string ErrorText() const;

    virtual void StartCancel();

    // client-side method
    void SetTimeout(int time);

    int GetTimeout();
    
    const std::string& RemoteReason() const;

    void SetRemoteReason(std::string);

    const std::string& LocalReason() const;

    void SetMethodName(const std::string method_name);

    const std::string& GetMethodName();
    
    void SetServiceName(const std::string service_name);
    
    const std::string& GetServiceName();
    
    void Done(std::string reason, bool failed);
    
    void SetDoneCallBack(callback func);
    
    void SetSync();
    
    bool IsSync();
    
    void StartTime();
    
    void Wait();
    
    void Signal();
    
    void SetRemoteEndPoint(const tcp::endpoint&);

    void SetSendMessage(const ReadBufferPtr& sendbuf);

    ReadBufferPtr& GetSendMessage();
    
    const tcp::endpoint GetRemoteEndPoint();

    // Server-side methods ---------------------------------------------

    virtual bool IsCanceled() const;

    virtual void NotifyOnCancel(google::protobuf::Closure* callback);

    virtual void SetFailed(const std::string& reason);

    void SetSuccess(const std::string& reason);

    void SetSeverStream(const RpcServerStreamPtr& server_stream);

    RpcServerStreamPtr GetSeverStream();

    // common
    void SetRequest(google::protobuf::Message* request);

    google::protobuf::Message* GetRequest();

    void SetResponse(google::protobuf::Message* response);

    google::protobuf::Message* GetResponse();
    
    bool IsDone();

    void SetSequenceId(uint64_t);
    
    uint64_t GetSequenceId();

private:
    // client
    bool _failed;
    bool _is_sync;
    callback _callback;
    std::mutex _mutex;
    std::condition_variable _cond;
    ReadBufferPtr _send_buf;
    int _timeout;

    // server
    RpcServerStreamPtr _server_stream;

    // common
    std::atomic<bool> _done;
    std::string _remote_reason;
    std::string _local_reason;
    uint64_t _sequence_id;
    boost::asio::ip::tcp::endpoint _remote_endpoint;
    boost::asio::ip::tcp::endpoint _local_endpoint;
    std::string _method_name;
    std::string _service_name;
    google::protobuf::Message* _response;
    google::protobuf::Message* _request;
};
}

#endif