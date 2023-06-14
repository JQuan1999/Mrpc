#include<mrpc/server/rpc_request.h>

namespace mrpc
{
RpcRequest::RpcRequest(const RpcHeader& header, const ReadBufferPtr& read_buf)
{
    _header = header;
    _read_buf = read_buf;
}

void RpcRequest::Parse(const RpcServerStreamPtr& stream, const ServicePoolPtr& service_pool)
{
    int meta_size = _header.meta_size;
    _meta_buf = _read_buf->Split(meta_size);
    _data_buf = _read_buf;

    if(!_meta.ParseFromZeroCopyStream(_meta_buf.get()))
    {
        std::string meta_string = _meta_buf->ToString();
        LOG(ERROR, "Parse(): receive meta buf is parse error, meta buf data: %s", meta_string.c_str());
        SendFailedMessage(stream, "receive meta parse error");
        return;
    }

    RpcMeta_Type type = _meta.type();
    if(type != RpcMeta_Type_REQUEST)
    {
        LOG(ERROR, "Parse(): receive type is not request");
        SendFailedMessage(stream, "receive type is not request");
        return;
    }

    std::string svc_name = _meta.service();
    std::string mth_name = _meta.method();

    ServiceBoard* svc_board = service_pool->GetServiceBoard(svc_name);
    if(svc_board == nullptr)
    {
        LOG(ERROR, "service name:%s is not existed", svc_name.c_str());
        SendFailedMessage(stream, "service name is not existed");
        return;
    }
    google::protobuf::Service* svc = svc_board->GetService();
    
    MethodBorad* mth_board = svc_board->GetMethodBoard(mth_name);
    if(mth_board == nullptr)
    {
        LOG(ERROR, "method name:%s is not existed", mth_name.c_str());
        SendFailedMessage(stream, "method name is not existed");
        return;
    }
    const google::protobuf::MethodDescriptor* method = mth_board->GetDescriptor();

    google::protobuf::Message* request = svc->GetRequestPrototype(method).New();
    if(!request->ParseFromZeroCopyStream(_data_buf.get()))
    {
        std::string data_str = _data_buf->ToString();
        LOG(ERROR, "request parse error, data buf: %s", data_str.c_str());
        SendFailedMessage(stream, "request parse error");
        delete request;
        return;
    }
    
    google::protobuf::Message* response = svc->GetResponsePrototype(method).New();
    RpcController* controller = new RpcController();
    controller->SetSeverStream(stream);
    controller->SetResponse(response);
    controller->SetRequest(request);
    controller->SetRemoteEndPoint(stream->GetRemote());
    controller->SetServiceName(svc_name);
    controller->SetMethodName(mth_name);

    CallMethod(svc, method, controller, request, response);
}


void RpcRequest::CallMethod(google::protobuf::Service* service,
                            const google::protobuf::MethodDescriptor* method, 
                            RpcController* controller, 
                            google::protobuf::Message* request,
                            google::protobuf::Message* response)
{
    RpcServerStreamPtr stream = controller->GetSeverStream();
    if(!stream.get())
    {
        LOG(ERROR, "Parse(): stream is nullptr maybe client has closed with timeout");
        delete controller;
        delete request;
        delete response;
        return;
    }
    google::protobuf::Closure* done = google::protobuf::NewCallback<RpcRequest, RpcController*>(this, &RpcRequest::CallBack, controller);
    service->CallMethod(method, controller, request, response, done);
}


void RpcRequest::CallBack(RpcController* controller)
{
    // Todo检查是否超时
    // timeout check()

    RpcServerStreamPtr stream = controller->GetSeverStream();
    if(controller->Failed())
    {
        LOG(ERROR, "CallBack(): %s call method: %s:%s failed: %s", 
            EndPointToString(controller->GetRemoteEndPoint()).c_str(), 
            controller->GetServiceName().c_str(), controller->GetMethodName().c_str(),
            controller->ErrorText().c_str());
        SendFailedMessage(stream, controller->ErrorText()); // callmethod失败
    }else
    {
        LOG(DEBUG, "CallBack(): %s call method: %s:%s succed: %s", 
            EndPointToString(controller->GetRemoteEndPoint()).c_str(), 
            controller->GetServiceName().c_str(), controller->GetMethodName().c_str(),
            controller->ErrorText().c_str());
        SendSuccedMessage(stream, controller); // callmethod成功
    }

    google::protobuf::Message* request = controller->GetRequest();
    google::protobuf::Message* response = controller->GetResponse();
    delete request;
    delete response;
    delete controller;
}

void RpcRequest::SendFailedMessage(const RpcServerStreamPtr& stream, std::string reason)
{
    RpcMeta meta;
    meta.set_type(RpcMeta_Type_RESPONSE);
    meta.set_sequence_id(_meta.sequence_id());
    meta.set_failed(true);
    meta.set_reason(reason);

    RpcHeader header;
    ReadBufferPtr readbuf;
    WriteBufferPtr writebuf;
    // 头部保留空间后面确定meta和response的大小后在保留位置写入header
    int header_size = sizeof(header);
    int pos = writebuf->Reserve(header_size); // pos为待写入的位置即writebuf已有字节数
    
    if(!meta.SerializeToZeroCopyStream(writebuf.get()))
    {
        LOG(ERROR, "meta serialize failed");
        stream->SendResponse(readbuf);
    }
    int meta_size = writebuf->ByteCount() - header_size - pos;

    header.meta_size = meta_size;
    header.data_size = 0;
    header.message_size = meta_size;
    writebuf->SetData(pos, reinterpret_cast<char*>(&header), header_size);
    writebuf->SwapOut(readbuf.get());
    stream->SendResponse(readbuf);
}

void RpcRequest::SendSuccedMessage(const RpcServerStreamPtr& stream, RpcController* controller)
{
    RpcMeta meta;
    meta.set_type(RpcMeta_Type_RESPONSE);
    meta.set_sequence_id(_meta.sequence_id());
    meta.set_failed(false);
    RpcHeader header;
    ReadBufferPtr readbuf;
    WriteBufferPtr writebuf;

    int header_size = sizeof(header);
    int pos = writebuf->Reserve(header_size);

    if(!meta.SerializeToZeroCopyStream(writebuf.get()))
    {
        LOG(ERROR, "meta serialize failed");
        stream->SendResponse(readbuf);
    }
    int meta_size = writebuf->ByteCount() - header_size - pos;

    google::protobuf::Message* respone = controller->GetResponse();
    if(!respone->SerializeToZeroCopyStream(writebuf.get()))
    {
        LOG(ERROR, "response serialize failed");
        stream->SendResponse(readbuf);
    }
    int data_size = writebuf->ByteCount() - pos - header_size - meta_size;

    header.meta_size = meta_size;
    header.data_size = data_size;
    header.message_size = meta_size + data_size;

    writebuf->SetData(pos, reinterpret_cast<char*>(&header), header_size);
    writebuf->SwapOut(readbuf.get());
    stream->SendResponse(readbuf);
}
} // namespace mrpc