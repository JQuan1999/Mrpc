#include<mrpc/server/rpc_request.h>

namespace mrpc
{
RpcRequest::RpcRequest(const RpcHeader& header, const ReadBufferPtr& read_buf)
{
    _header = header;
    _read_buf = read_buf;
}

void RpcRequest::Split()
{
    int meta_size = _header.meta_size;
    _meta_buf = _read_buf->Split(meta_size);
    _data_buf = _read_buf;
}

void RpcRequest::Parse(const ServicePoolPtr& service_pool)
{
    Split();
    RpcMeta meta;
    if(!meta.ParseFromZeroCopyStream(_meta_buf.get()))
    {
        std::string meta_string = _meta_buf->ToString();
        LOG(ERROR, "Parse(): receive meta buf is parse error, meta buf data: %s", meta_string.c_str());
    }
}
} // namespace mrpc