#ifndef _MRPC_REQUEST_H
#define _MRPC_REQUEST_H

#include<deque>
#include<functional>
#include<google/protobuf/stubs/callback.h>

#include<mrpc/common/rpc_controller.h>
#include<mrpc/common/buffer.h>
#include<mrpc/proto/rpc_meta.pb.h>
#include<mrpc/proto/rpc_header.h>
#include<mrpc/server/rpc_server_stream.h>
#include<mrpc/server/service_pool.h>

namespace mrpc
{
class RpcRequest
{
public:
    RpcRequest(RpcHeader header, const ReadBufferPtr& read_buf);
    void Parse(const RpcServerStreamPtr& stream, const ServicePoolPtr& service_pool);

    void CallMethod(google::protobuf::Service* service,
                    const google::protobuf::MethodDescriptor* method, 
                    RpcController* controller, 
                    google::protobuf::Message* request, 
                    google::protobuf::Message* response);

    void CallBack(RpcController* controller);

    void SendFailedMessage(const RpcServerStreamPtr& stream, std::string reason);

    void SendSuccedMessage(const RpcServerStreamPtr& stream, RpcController* controller);
private:
    RpcHeader _header;
    RpcMeta _meta;
    ReadBufferPtr _read_buf;
    ReadBufferPtr _meta_buf;
    ReadBufferPtr _data_buf;
};
}
#endif