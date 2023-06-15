#ifndef _MRPC_CHANNEL_H_
#define _MRPC_CHANNEL_H_

#include<google/protobuf/service.h>
#include<vector>
#include<memory>
namespace mrpc{

class RpcChannel: public google::protobuf::RpcChannel{
public:
    RpcChannel(){};

    virtual ~RpcChannel(){};

    // 初始化 解析address
    virtual bool Init() = 0;

    // stop the channel
    virtual void Stop() = 0;

    // call method
    virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                            ::google::protobuf::RpcController* controller,
                            const ::google::protobuf::Message* request,
                            ::google::protobuf::Message* response,
                            ::google::protobuf::Closure* done) = 0;

    // 获得还未完成的调用数量
    virtual uint32_t WaitCount() = 0;
};
}

#endif