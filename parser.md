# 服务端解析的过程：
```
    virtual void on_read_some(
            const boost::system::error_code& error,
            std::size_t bytes_transferred)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        // 调用byte_stream的is_connected
        if (!is_connected()) return;

        // error发生
        if (error)
        {   
            // 判断error是不是断开连接
            close(error.message());
            return;
        }

        _last_rw_ticks = _ticks; // 修改rw_ticks
        ++_total_received_count; // 累计收到的数量+1
        _total_received_size += bytes_transferred; // 更新累计收到的字节数

        // 分割并处理数据 split_and_process_message包含两种解析策略，一种是http一种是自定义协议，根据头部首四字节区分get,post和自定义协议
        // 
        std::deque<RpcRequestPtr> received_messages;
        if (!split_and_process_message(_receiving_data,
                    static_cast<int>(bytes_transferred), &received_messages))
        {
            close("broken stream");
            return;
        }

        _receiving_data += bytes_transferred; // 指针偏移
        _receiving_size -= bytes_transferred; // 更新剩余数据

        // check if current tran buf is used out
        // 如果剩余数据已读完并且申请新内存失败 out of memory
        if (_receiving_size == 0 && !reset_tran_buf())
        {
            close("out of memory");
            return;
        }

        // 释放token继续读 release token
        atomic_comp_swap(&_receive_token, TOKEN_FREE, TOKEN_LOCK);

        // trigger next receive
        try_start_receive();

        // process messages 解析到的received_messages即requestptr非空，调用request的处理方法
        // request的类型包含：http_request和binayry_request
        while (!is_closed() && !received_messages.empty())
        {
            on_received(received_messages.front()); // 最后会调用request的ProcessRequest函数
            received_messages.pop_front();
        }
    }
```

split_and_process函数：返回true表示解析过程没有出错，结果在received_messages中保存(可能为空)，返回false表示解析出错
```
    bool split_and_process_message(char* data, int size,
            std::deque<RpcRequestPtr>* received_messages)
    {
        SOFA_PBRPC_FUNCTION_TRACE;

        while (size > 0)
        {
            int consumed;
            if (_magic_string_recved_bytes < 4)
            {
                // 解析头部四字节，选择对应的解析策略 如果读到的头部不完全返回true
                // 如果没有对应的解析策略返回false表示解析出错
            }
            // 解析数据 _tran_buf强指针
            // 调用解析策略的Parse函数：返回值ret = 0请求不完整 ret = -1解析出错 ret = 1解析到一条完整的消息
            int ret = _current_rpc_request_parser->Parse(
                    _tran_buf, size, data - _tran_buf, &consumed);
            _cur_recved_bytes += consumed;
            if (ret == 0)
            {
                return true;
            }
            else if (ret < 0)
            {
                return false;
            }
            // 返回值为1表示解析到了一条完整的消息
            data += consumed;// data偏移
            size -= consumed;// 减去已读取的字节
            RpcRequestPtr request = _current_rpc_request_parser->GetRequest(); // 获取request
            // 设置request的信息：remote_endpoint、接收时间等
            // 接收到了一条完整的消息，清空临时变量继续解析剩余size的数据
        }
        return true;
    }
```

# 不同的解析策略
## 抽象类
```

class RpcRequestParser
{
public:
    RpcRequestParser() {}
    virtual ~RpcRequestParser(){}

    // Get the parser name.
    virtual const char* Name() = 0;

    // 状态重置
    virtual void Reset() = 0;

    // 检查区分的头四个字节
    virtual bool CheckMagicString(const char* magic_string) = 0;

    // Parse received data.
    //
    // @param buf：数据的起始地址
    // @param data_size：数据大小
    // @param offset：待解析数据的起始地址
    // @param bytes_consumed：消耗的数据大小
    // 返回值1：数据已准备好，返回值0数据不完全，返回值-1解析出错
    virtual int Parse(const char* buf, int data_size, int offset, int* bytes_consumed) = 0;

    // 获取request，前提条件是：Parse已经返回1
    virtual RpcRequestPtr GetRequest() = 0;

public:
    // Get all registered parsers.
    static void RegisteredParsers(std::vector<RpcRequestParserPtr>* parsers);
}; // class RpcRequestParser
```

## Http请求解析类
```
class HTTPRpcRequestParser : public RpcRequestParser
{
// 主要的成员变量：
private:
    enum ParseState // 状态
    {
        PS_METHOD,
        PS_PATH,
        PS_HTTP_VERSION,
        PS_EXPECT_NEW_LINE1,
        PS_HEAD_LINE_START,
        PS_HEAD_NAME,
        PS_HEAD_COLON,
        PS_HEAD_VALUE,
        PS_EXPECT_NEW_LINE2,
        PS_BODY
    };
    ParseState _state; // current parsing state
    std::string _header_name; // current parsing header name
    std::string _header_value; // currrent parsing header value
    int64 _content_length; // body content length
    HTTPRpcRequestPtr _req; // 请求体
    static const std::string CONTENT_LENGTH;
    static const std::string ACCEPT;
    static const std::string ACCEPT_PROTOBUF;
}

主要函数：
int HTTPRpcRequestParser::Parse(const char* buf,
        int data_size, int offset, int* bytes_consumed)
{
    *bytes_consumed = 0;
    // 状态不等于请求体
    if (_state != PS_BODY)
    {
        const char* ptr = buf + offset; // 指向待解析的数据位置
        const char* end = buf + offset + data_size; // 结束位置
        std::string err;
        for (; ptr != end; ++ptr)
        {
            // 解析当前字节 根据状态机进行解析
            // 返回值=1解析到请求体，返回值0表示继续，返回值-1表示解析错误
            int ret = ParseInternal(*ptr, err);
            if (ret < 0)
            {
                return ret;
            }
            if (ret > 0)  // header解析完
            {
                ++ptr;
                break;
            }
        }
        *bytes_consumed = ptr - (buf + offset); // 消耗的字节数
        data_size -= *bytes_consumed; // 减去已消耗的字节数等于剩余的数据
        offset += *bytes_consumed; // 更新位移大小
    }
    if (_state == PS_BODY) // 状态为请求体
    {
        // content长度减去已收到的字节数等于剩余字节数
        int bytes_remain = _content_length - _req->_req_body->TotalCount(); 
        if (bytes_remain == 0)
        {
            return 1; // 剩余数据为0返回1表示接收到了完整请求
        }
        if (data_size == 0)
        {
            return 0; // 剩余数据为0 返回0请求不完整
        }
        int consume = std::min(data_size, bytes_remain); // 取min表示可以取出的数据大小
        _req->_req_body->Append(BufHandle(const_cast<char*>(buf), consume, offset));
        *bytes_consumed += consume;
        // 已收到的字节等于content长度表示收到一条完整的消息
        if (_content_length == _req->_req_body->TotalCount())
        {
            return 1;
        }
        return 0;
    }
    return 0;
}
```

## 自定义协议解析类
```
class BinaryRpcRequestParser : public RpcRequestParser
{
// 主要成员变量
private:
    {
        PS_MAGIC_STRING,
        PS_MSG_HEADER,
        PS_MSG_BODY
    };
    ParseState _state; // 解析状态
    int _bytes_recved; // 已接收的数据
    BinaryRpcRequestPtr _req; // 消息体

    static const int64 MAX_MESSAGE_SIZE;
}

// 主要函数：
int BinaryRpcRequestParser::Parse(const char* buf,
        int data_size, int offset, int* bytes_consumed)
{
    if (data_size == 0)
    {
        return 0;
    }
    *bytes_consumed = 0;
    int64 bytes_remain, consume;
    switch (_state)
    {
        case PS_MSG_HEADER: // 解析头部
            bytes_remain = sizeof(RpcMessageHeader) - _bytes_recved; // 剩余头部数据 = 头部大小 - 已接收的数据大小
            
            // data_size有可能大于剩余头部数据也有可能小于剩余头部数据所以取min表示可以读取的数据
            consume = std::min(static_cast<int64>(data_size), bytes_remain);

            // 将buf + offset开始的consume拷贝到header中
            memcpy(reinterpret_cast<char*>(&_req->_req_header) + _bytes_recved,
                    buf + offset, consume);

            *bytes_consumed += consume; // 消耗的数据 + consume
            _bytes_recved += consume; // 当前parse收到的累计数据 + consume

            if (_bytes_recved < static_cast<int>(sizeof(RpcMessageHeader)))
            {
                // 如果收到的数据小于一个头部 表示头部不完全
                return 0;
            }
            if (_req->_req_header.message_size == 0)
            {
                // 如果消息大小为0没有剩余数据 直接返回
                return 1;
            }

            // header complete
            data_size -= consume; // 更新剩余待读取数据
            offset += consume; // 更新偏位移
            _state = PS_MSG_BODY; // 进入消息体的解析
            _bytes_recved = 0; // 将已接受的大小重置为0 表示已接受的消息体字节数
            if (data_size == 0)
            {
                return 0;
            }
        case PS_MSG_BODY:
            // 头部的_req_header.message_size - 已接受的数据为message的剩余数据
            bytes_remain = _req->_req_header.message_size - _bytes_recved;

            // data_size和bytes_remain 取min为需要解析的数据
            // data_size可能包含多条的信息 也有可能包含一条或一条的部分数据 因此需要取min
            consume = std::min(static_cast<int64>(data_size), bytes_remain);

            // 将buf组装成一个BufHandle append到readbuf中
            _req->_req_body->Append(BufHandle(const_cast<char*>(buf), consume, offset));

            // 更新消耗的数据和总共读取的数据
            *bytes_consumed += consume;
            _bytes_recved += consume;
            // 如果读到一条完整的消息返回1
            if (_bytes_recved == _req->_req_header.message_size)
            {
                // 反序列化得到meta
                if (!_req->_req_meta.ParseFromBoundedZeroCopyStream(
                            _req->_req_body.get(), _req->_req_header.meta_size))
                {
                    // parse meta failed
                    SLOG(ERROR, "Parse(): %s: parse rpc meta failed",
                            RpcEndpointToString(_req->_remote_endpoint).c_str());
                    return -1;
                }
                return 1;
            }
            // 否则返回0
            return 0;
        default:
            SCHECK(false);
            return -1;
    }
}
```

# Request类
## 抽象RpcRequest类
```
class RpcRequest
{
// 请求类型
enum RpcRequestType
{
    BINARY,
    HTTP
};
public:
    // 主要成员函数
    // HTTP或Binary request
    virtual RpcRequestType RequestType() = 0;

    // 抽象接口：处理接收到的Request 子类实现
    virtual void ProcessRequest(
            const RpcServerStreamWPtr& server_stream,
            const ServicePoolPtr& service_pool) = 0;

    // 抽象接口：组装成功的response到cntl的Buffer
    virtual ReadBufferPtr AssembleSucceedResponse(
            const RpcControllerImplPtr& cntl,
            const google::protobuf::Message* response,
            std::string& err) = 0;

    // 抽象接口：组装失败的response到cntl的Buffer
    virtual ReadBufferPtr AssembleFailedResponse(
            int32 error_code,
            const std::string& reason,
            std::string& err) = 0;

private:
    void CallMethod(
            MethodBoard* method_board,
            RpcController* controller,
            google::protobuf::Message* request,
            google::protobuf::Message* response);

    void OnCallMethodDone(
        MethodBoard* method_board,
        RpcController* controller,
        google::protobuf::Message* request,
        google::protobuf::Message* response);
}

// 具体的子类中调用
// 调用service的方法并将结果request发送给客户端
void RpcRequest::CallMethod(
        MethodBoard* method_board,
        RpcController* controller,
        google::protobuf::Message* request,
        google::protobuf::Message* response)
{
    RpcServerStreamPtr stream = cntl->RpcServerStream().lock(); // 获取对应的cntl的stream连接

    // 回调函数当调用完毕在service中执行回调函数done 将request发送出去
    google::protobuf::Closure* done = NewClosure(
            shared_from_this(), &RpcRequest::OnCallMethodDone,
            method_board, controller, request, response);
    
    if (cntl->ServerTimeout() > 0)
    {
        // 检查超时时间如果已超时 时间计算是从接收到完整的request为开始时间
        // 用当前时间减去接收到的时间为等待时间t，如果t大于设置的超时时间则一定超时，如果小于有可能超时
        // 因为客户端发送的时候就已经开启了定时器，中间还经过了网络的发送时间
        // 对于已超时的数据包没必要发送了，因为客户端已经将请求标记为超时了，可以减小网络带宽
        int64 server_wait_time_us =
            (time_now - cntl->RequestReceivedTime()).total_microseconds();
        if (server_wait_time_us > cntl->ServerTimeout() * 1000)
        {
            delete request;
            delete response;
            delete controller;
            return;
        }
    }

    // 调用method_board的service方法
    method_board->GetServiceBoard()->Service()->CallMethod(
            method_board->Descriptor(), controller, request, response, done);
}

回调函数发送
void RpcRequest::OnCallMethodDone(
        MethodBoard* method_board,
        RpcController* controller,
        google::protobuf::Message* request,
        google::protobuf::Message* response)
{
    RpcServerStreamPtr stream = cntl->RpcServerStream().lock(); // 获取stream连接
    
    // 检查是否超时超时则直接返回
    if (cntl->Failed())
    {
        // 调用子类的方法组装失败的response并发送到stream对端
        SendFailedResponse(cntl->RpcServerStream(), cntl->ErrorCode(), cntl->Reason());
    }
    else
    {
        // 调用子类的方法组装请求成功的response并发送stream对端
        SendSucceedResponse(cntl, response); 
    }

    // 回调函数完成删除指针
    delete request; 
    delete response;
    delete controller;
}
```

## HttpRequest
```
class HTTPRpcRequest : public RpcRequest
{
public:
    // 主要成员函数
    virtual RpcRequestType RequestType();

    virtual void ProcessRequest(
            const RpcServerStreamWPtr& server_stream,
            const ServicePoolPtr& service_pool);

    virtual ReadBufferPtr AssembleSucceedResponse(
        const RpcControllerImplPtr& cntl,
        const google::protobuf::Message* response,
        std::string& err);

    virtual ReadBufferPtr AssembleFailedResponse(
        int32 error_code,
        const std::string& reason,
        std::string& err);
private:
    // 主要成员变量
    // 回复的类型1. json 2.protobuf 3. html
    enum RenderType
    {
        JSON = 1,
        PROTOBUF = 2,
        HTML = 3
    };
    // Http请求的类型 1. get 2. post 3. post请求 请求体为Protobuf格式
    enum Type
    {
        GET = 0,
        POST = 1,
        POST_PB = 2
    };
    Type                               _type; // 请求类型
    std::string                        _original_path; // get请求传递的请求路径，因为get的参数是通过url传递的
    std::string                        _path; // 调用ParsePath解析得到实际的请求路径
    std::string                        _query_string; // 调用ParsePath解析得到请求参数string
    std::map<std::string, std::string> _query_params; // 通过对_query_string的&进行划分，再通过=划分请求key和value
    std::string                        _http_version; // http版本
    std::map<std::string, std::string> _headers; // 头部字段
    ReadBufferPtr                      _req_body; // 请求体
    rapidjson::Document*               _req_json;
}

bool HTTPRpcRequest::ParsePath()
{
    // 对请求路径进行解析 因为Get请求的参数是通过url传递的
    // 例如访问https://host:port/path?xxx=aaa&ooo=bbb
    // _original_path = /path?xxx=aaa&ooo=bbb
    // 解析得到_path = path
    // _query_string = xxx=aaa&ooo=bbb
    // _query_params = {{xxx, aaa}, {ooo, bbb}};
}

// 处理request 将response发送给客户端
void HTTPRpcRequest::ProcessRequest(
        const RpcServerStreamWPtr& server_stream,
        const ServicePoolPtr& service_pool)
{
    std::string service_name;
    std::string method_name;
    ParseMethodFullName(_method, &service_name, &method_name); // 获取服务名和方法名

    google::protobuf::Service* service = method_board->GetServiceBoard()->Service(); // 通过服务名和方法名找到对应的service
    const google::protobuf::MethodDescriptor* method_desc = method_board->Descriptor();

    google::protobuf::Message* request = service->GetRequestPrototype(method_desc).New();
    std::string json_str;
    if (_type == POST)
    {
        json_str = _req_body->ToString(); // post参数在请求体中
    }
    else
    {
        json_str = _query_params["request"]; // get参数为_query_params["request"]
    }
    if (json_str.empty())
    {
        json_str = "{}"; // 没有请求参数直接初始化空的json
    }

    std::string err;
    _req_json = ParseJson(json_str.c_str(), err); // _解析请求参数 req_json为成员变量 rapidjson::Document*类型 

    // json转为protobuf::message的request 因为service函数只接受protobuf::message的参数
    if (_req_json == NULL || jsonobject2pb(_req_json, request, err) < 0);

    google::protobuf::Message* response = service->GetResponsePrototype(method_desc).New();
    RpcController* controller = new RpcController();
    // 设置cntl的参数：方法、endpoint、stream、接收时间、http请求参数、http头部字段

    CallMethod(method_board, controller, request, response); // 最后调用继承自抽象类的方法
}

// 组装请求成功的response
ReadBufferPtr HTTPRpcRequest::AssembleSucceedResponse(
        const RpcControllerImplPtr& /*cntl*/,
        const google::protobuf::Message* response,
        std::string& err)
{
    WriteBuffer write_buffer;
    std::string json_str;
    pb2json(response, json_str); // 将response的protobuf格式数据转为json格式数据 发送给客户端
    if (!RenderResponse(&write_buffer, JSON, json_str)) // 将json格式的数据 组装成完整的http回复消息
    {
        err = "render json response failed";
        return ReadBufferPtr();
    }

    ReadBufferPtr read_buffer(new ReadBuffer());
    write_buffer.SwapOut(read_buffer.get());
    return read_buffer;
}

// param: output为writebuf
// param: type为回复类型
// param: body为response序列化得到的string
bool HTTPRpcRequest::RenderResponse(
        google::protobuf::io::ZeroCopyOutputStream* output,
        const RenderType type,
        const std::string& body)
{
    std::ostringstream oss;
    oss << body.size(); // 
    google::protobuf::io::Printer printer(output, '$');
    printer.Print("HTTP/1.1 200 OK\r\n");
    printer.Print("Content-Type: application/json\r\n");

    printer.Print("Access-Control-Allow-Origin: *\r\n");
    
    printer.Print("Content-Length: $LENGTH$\r\n", "LENGTH", oss.str()); // response的长度
    
    printer.Print("\r\n");

    printer.PrintRaw(body);
    return !printer.failed();
}

// 组装失败的response
ReadBufferPtr HTTPRpcRequest::AssembleFailedResponse(
        int32 error_code,
        const std::string& reason,
        std::string& err)
{
    // oss组装请求失败消息体
    std::ostringstream oss;
    oss << "\"ERROR: " << error_code << ": "
        << StringUtils::replace_all(reason, "\"", "\\\"") << "\"";

    WriteBuffer write_buffer;
    // 以JSON类型回复
    if (!RenderResponse(&write_buffer, JSON, oss.str()))
    {
        err = "render json response failed";
        return ReadBufferPtr();
    }

    ReadBufferPtr read_buffer(new ReadBuffer());
    write_buffer.SwapOut(read_buffer.get());
    return read_buffer;
}

bool HTTPRpcRequest::RenderResponse(
        google::protobuf::io::ZeroCopyOutputStream* output,
        const RenderType type,
        const std::string& body)
{
    std::ostringstream oss;
    oss << body.size(); // 消息体

    google::protobuf::io::Printer printer(output, '$');
    printer.Print("HTTP/1.1 200 OK\r\n");
    printer.Print("Content-Type: application/json\r\n");
    printer.Print("Access-Control-Allow-Origin: *\r\n");
    printer.Print("Content-Length: $LENGTH$\r\n", "LENGTH", oss.str()); // response的长度
    printer.Print("\r\n");
    printer.PrintRaw(body);
    return !printer.failed();
}
```

## BinaryRequest
```
class BinaryRpcRequest : public RpcRequest
{
public:
    // 主要成员函数
    virtual void ProcessRequest(
            const RpcServerStreamWPtr& server_stream,
            const ServicePoolPtr& service_pool);

    virtual ReadBufferPtr AssembleSucceedResponse(
            const RpcControllerImplPtr& cntl,
            const google::protobuf::Message* response,
            std::string& err);

    virtual ReadBufferPtr AssembleFailedResponse(
            int32 error_code,
            const std::string& reason,
            std::string& err);
private:
    // 主要成员变量
    RpcMessageHeader _req_header; // header
    RpcMeta          _req_meta; // meta
    ReadBufferPtr    _req_body; // buffer_ptr 消息体
}

void BinaryRpcRequest::ProcessRequest(
        const RpcServerStreamWPtr& stream,
        const ServicePoolPtr& service_pool)
{
    std::string service_name;
    std::string method_name;
    ParseMethodFullName(_req_meta.method(), &service_name, &method_name); // 解析得到服务名和方法名

    MethodBoard* method_board = FindMethodBoard(service_pool, service_name, method_name); // 找到对应的service
    google::protobuf::Service* service = method_board->GetServiceBoard()->Service();
    const google::protobuf::MethodDescriptor* method_desc = method_board->Descriptor();

    google::protobuf::Message* request = service->GetRequestPrototype(method_desc).New(); // 创建request
    request->ParseFromZeroCopyStream(_req_body.get()); // 将二进制数据req_body反序列化request

    google::protobuf::Message* response = service->GetResponsePrototype(method_desc).New();

    // 新new一个controller
    RpcController* controller = new RpcController();
    const RpcControllerImplPtr& cntl = controller->impl();
    // 设置cntl的信息：_local_endpoint、_remote_endpoint、stream、server_timeout、_received_time

    // 调用父类的CallMethod
    CallMethod(method_board, controller, request, response);
}

// 组装请求成功的消息
ReadBufferPtr BinaryRpcRequest::AssembleSucceedResponse(
        const RpcControllerImplPtr& cntl,
        const google::protobuf::Message* response,
        std::string& err)
{
    // 自定义协议的消息组装 跟客户端自定义协议发送的流程一致
    // 1. 设置头部
    // 2. 设置meta信息 序列化到writebuf
    // 3. 设置response信息 序列化到writebuf
    // 最后根据2，3两部的序列化结果设置头部字段meta_size和data_size
}

// 组装请求失败的消息
ReadBufferPtr BinaryRpcRequest::AssembleFailedResponse(
        int32 error_code,
        const std::string& reason,
        std::string& err)
{
    // 与AssembleSucceedResponse一致，但是没有response信息需要设置meta的error字段
}
```

# 调用流程图
![](./docs/4.JPG)