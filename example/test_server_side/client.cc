#include"test.pb.h"
#include<boost/asio.hpp>
#include<mrpc/common/end_point.h>
#include<mrpc/common/buffer.h>
#include<mrpc/proto/rpc_header.h>
#include<mrpc/proto/rpc_meta.pb.h>

using namespace test;
using namespace mrpc;
using namespace boost::asio::ip;

int sequence_id = 0;

bool CallMethod(boost::asio::io_context& ioc, tcp::socket& sock, std::string service, std::string method, std::string request_data, int times=-1)
{
    Request request;
    request.set_request(request_data);
    if(times != -1){
        request.set_times(times);
    }
    LOG(INFO, "request: %s", request.request().c_str());

    WriteBufferPtr writebuf(new WriteBuffer());
    RpcHeader header;
    int header_size = sizeof(header);
    int pos = writebuf->Reserve(header_size);

    RpcMeta meta;
    meta.set_type(RpcMeta::REQUEST); // 设置为request类型
    meta.set_sequence_id(sequence_id++); // 设置本次request id
    meta.set_service(service);
    meta.set_method(method);

    if(!meta.SerializeToZeroCopyStream(writebuf.get()))
    {
        LOG(ERROR, "serialized rpc meta data failed");
        return false;
    }
    
    int meta_size = writebuf->ByteCount() - pos - header_size; // meta size
    
    if(!request.SerializeToZeroCopyStream(writebuf.get()))
    {
        LOG(ERROR, "serialized request data failed");
        return false;
    }
    int data_size = writebuf->ByteCount() - pos - header_size - meta_size;

    header.meta_size = meta_size;
    header.data_size = data_size;
    header.message_size = meta_size + data_size;
    writebuf->SetData(pos, reinterpret_cast<char*>(&header), header_size);
    ReadBufferPtr readbuf_ptr(new ReadBuffer());
    writebuf->SwapOut(readbuf_ptr.get());

    LOG(INFO, "message compose success now to send meta_size: %d data_size: %d message_size: %d", meta_size, data_size, meta_size + data_size);
    
    const void* data;
    int size = 0;
    boost::system::error_code ec;
    while(readbuf_ptr->Next(&data, &size))
    {
        sock.write_some(boost::asio::buffer((char*)data, size), ec);
        if(ec){
            LOG(ERROR, "write to server failed error msg: %s", ec.message().c_str());
            return false;
        }
    }
    ReadBuffer readbuf;

    sock.read_some(boost::asio::buffer(reinterpret_cast<char*>(&header), sizeof(header)), ec);
    if(ec){
        LOG(ERROR, "read header failed error msg: %s", ec.message().c_str());
        return false;
    }else{
        meta_size = header.meta_size;
        data_size = header.data_size;
        LOG(DEBUG, "read header success message meta_size: %d data_size: %d", meta_size, data_size);
    }
    
    int factor_size = 1;
    while((BUFFER_UNIT << factor_size) < meta_size){
        factor_size++;
    }
    Buffer buf1(factor_size);
    sock.read_some(boost::asio::buffer(buf1.GetHeader(), meta_size), ec);
    // change to readbuf
    buf1.SetCapacity(meta_size);
    buf1.SetSize(0);
    if(ec){
        LOG(ERROR, "read meta data failed error msg: %s", ec.message().c_str());
        return false;
    }

    readbuf.Append(buf1);
    if(!meta.ParseFromZeroCopyStream(&readbuf))
    {
        LOG(ERROR, "parser meta data failed error msg: %s", ec.message().c_str());
        return false;
    }
    if(meta.failed())
    {
        LOG(INFO, "call method failed error reason: %s", meta.reason().c_str());
    }else{
        LOG(INFO, "call method success: seqenced id: %d", meta.sequence_id());
    }

    factor_size = 1;
    while((BUFFER_UNIT << factor_size) < data_size){
        factor_size++;
    }
    Buffer buf2(factor_size);
    sock.read_some(boost::asio::buffer(buf2.GetHeader(), data_size), ec);
    buf2.SetCapacity(data_size);
    buf2.SetSize(0);
    if(ec){
        LOG(ERROR, "read meta failed error msg: %s", ec.message().c_str());
        return false;
    }
    readbuf.Clear();
    readbuf.Append(buf2);
    
    Response Response;
    if(!Response.ParseFromZeroCopyStream(&readbuf))
    {
        LOG(ERROR, "parser response failed error msg: %s", ec.message().c_str());
        return false;
    }
    LOG(INFO, "return response is: %s", Response.response().c_str());
    return true;
}

void OnConnect(tcp::socket* sock_ptr, const boost::system::error_code& ec)
{
    if(ec)
    {
        LOG(ERROR, "OnConnect(): connect error errors msg: %s", ec.message().c_str());
        return;
    }
    std::string msg = "abcd";
    boost::system::error_code error;
    sock_ptr->write_some(boost::asio::buffer(msg, 4), error);
    if(error){
        LOG(ERROR, "OnConnect(): write_some errors msg: %s", error.message().c_str());
    }
}

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    boost::asio::io_context ioc;
    // boost::asio::io_context::work work(ioc);
    tcp::socket sock(ioc);
    tcp::endpoint endpoint(address::from_string("127.0.0.1"), 12345);
    boost::system::error_code ec;
    sock.connect(endpoint, ec);
    if(ec)
    {
        LOG(ERROR, "sock connect error msg: %s", ec.message().c_str());
        return -1;
    }
    tcp::endpoint local = sock.local_endpoint();
    LOG(INFO, "local address is: %s", EndPointToString(local).c_str());
    if(!CallMethod(ioc, sock, "UserService", "Tolower", "HELLO WORLD FROM JQ"))
    {
        return -1;
    }
    if(!CallMethod(ioc, sock, "UserService", "ToUpper", "hello world from jq"))
    {
        return -1;
    }
    if(!CallMethod(ioc, sock, "UserService", "Expand", "hello world from jq", 2))
    {
        return -1;
    }
    if(!CallMethod(ioc, sock, "UserService", "NotExistedMethod", "failed request"))
    {
        return -1;
    }
    ioc.run();
    LOG(INFO, "run done");
    return 0;
}