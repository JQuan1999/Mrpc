#ifndef _MRPC_REQUEST_H
#define _MRPC_REQUEST_H

#include<deque>

#include<mrpc/proto/rpc_meta.pb.h>
#include<mrpc/proto/rpc_header.h>
#include<mrpc/common/buffer.h>
#include<mrpc/server/service_pool.h>

namespace mrpc
{
class RpcRequest
{
public:
    RpcRequest(const RpcHeader& header, const ReadBufferPtr& read_buf)
    {
        _header = header;
        _read_buf = read_buf;
    }
    void Split();
    void Parse(const ServicePoolPtr& service_pool);

private:
    RpcHeader _header;
    ReadBufferPtr _read_buf;
    ReadBufferPtr _meta_buf;
    ReadBufferPtr _data_buf;
};
}
#endif