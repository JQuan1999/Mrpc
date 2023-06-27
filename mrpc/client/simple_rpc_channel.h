#ifndef _MRPC_SIMPLE_CHANNEL_H_
#define _MRPC_SIMPLE_CHANNEL_H_

#include<atomic>
#include<google/protobuf/service.h>
#include<google/protobuf/descriptor.h>
#include<google/protobuf/message.h>

#include<mrpc/client/rpc_channel.h>
#include<mrpc/client/mrpc_client.h>
#include<mrpc/client/mock_test.h>
#include<mrpc/common/logger.h>
#include<mrpc/common/rpc_controller.h>

namespace mrpc{

class RpcSimpleChannel;
typedef std::shared_ptr<RpcSimpleChannel> SimpleChannelPtr;

class RpcSimpleChannel: public RpcChannel, public std::enable_shared_from_this<RpcSimpleChannel>
{
public:
    RpcSimpleChannel(const RpcClientPtr& rpc_client_ptr, const std::string& address, uint32_t port);

    virtual ~RpcSimpleChannel();

    // 解析地址
    virtual bool Init();

    virtual void Stop();

    // call method
    virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const ::google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done);

    // 还未完成的调用数量
    virtual uint32_t WaitCount();

    bool ResovleSuccess();

public:
    void WaitDone(RpcController* crt);

    void DoneCallBack(google::protobuf::Closure* done, RpcControllerPtr ptr);

    static void MockDoneCallBack(RpcController* crt);

private:
    tcp::endpoint _remote_endpoint;
    RpcClientPtr _client_ptr;
    std::string _address;
    uint32_t _port;
    std::atomic<uint32_t> _wait_count;
    bool _resolve_success;
    bool _is_mock;
};

}

#endif