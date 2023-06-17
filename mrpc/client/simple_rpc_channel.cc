#include<mrpc/client/simple_rpc_channel.h>

namespace mrpc{

RpcSimpleChannel::RpcSimpleChannel(const RpcClientPtr& rpc_client_ptr, const std::string& address, uint32_t port)
    : _client_ptr(rpc_client_ptr)
    , _address(address)
    , _port(port)
    , _wait_count(0)
    , _resolve_success(false)
    , _is_mock(false)
{
    Init();
}

RpcSimpleChannel::~RpcSimpleChannel()
{
    LOG(DEBUG, "in ~RpcSimpleChannel()");
    Stop();
}

bool RpcSimpleChannel::Init()
{
    if(MockTest::GetSingleMockTest()->IsEnableMockTest() && _address.find(MOCK_TEST_ADDRESS) == 0)
    {
        LOG(INFO, "Init(): mock test");
        _is_mock = true;
        return true;
    }
    if(!ResovleAddress(_client_ptr->GetIoService(), _address, _port, &_remote_endpoint))
    {
        LOG(INFO, "Init(): resolve address failed");
        _resolve_success = false;
        return false;
    }
    else
    {
        LOG(INFO, "Init(): resolve address success, endpoint: [%s]", EndPointToString(_remote_endpoint).c_str());
        _resolve_success = true;
        return true;
    }
}

void RpcSimpleChannel::Stop()
{
    LOG(INFO, "Stop(): simple rpc channel stop: %s", _address.c_str());
}

void RpcSimpleChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller,
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf::Closure* done)
{
    ++_wait_count;
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string method_name = method->name();
    std::string service_name = sd->name();

    RpcController* cnt = dynamic_cast<RpcController*>(controller);
    cnt->SetMethodName(method_name);
    cnt->SetServiceName(service_name);
    // 回调函数当完成时调用channel的回调函数
    cnt->SetDoneCallBack(std::bind(&RpcSimpleChannel::DoneCallBack, shared_from_this(), 
                                   done, std::placeholders::_1));
    if(done == nullptr)
    {
        cnt->SetSync(); // 设置为同步调用
    }

    if(_is_mock)
    {
        std::string mock_method_name = service_name + ":" + method_name;
        auto method_func = MockTest::GetSingleMockTest()->FindMethod(mock_method_name);
        if(!method_func)
        {
            LOG(INFO, "CallMethod(): mock method:%s is not existed", mock_method_name.c_str());
            cnt->Done(mock_method_name+" is not existed", true);
        }
        else
        {
            LOG(INFO, "CallMethod(): mock method [%s] is called", mock_method_name.c_str());
            google::protobuf::Closure* mock_done = google::protobuf::NewCallback(&RpcSimpleChannel::MockDoneCallBack, cnt); // mock_done模拟收到服务端消息执行controller->Done()
            method_func(cnt, request, response, mock_done);
        }
        WaitDone(cnt);
        return;
    }
    if(!_resolve_success)
    {
        LOG(ERROR, "CallMethod(): resolve address failed: %s", _address.c_str());
        cnt->Done("solve address failed", true);
        WaitDone(cnt);
    }

    cnt->SetRemoteEndPoint(_remote_endpoint);
    cnt->StartTime(); // ToDo设置开始时间 超时管理
    _client_ptr->CallMethod(request, response, cnt);
    WaitDone(cnt);
}

uint32_t RpcSimpleChannel::WaitCount()
{
    return _wait_count.load();
}

// 同步调用阻塞等待完成
void RpcSimpleChannel::WaitDone(RpcController* cnt)
{
    if(cnt->IsSync())
    {
        cnt->Wait();
    }
}

// 回调函数根据同步调用还是异步调用, 同步调用唤醒阻塞在CallMethod的线程, 异步调用将done函数加入client的回调线程
void RpcSimpleChannel::DoneCallBack(google::protobuf::Closure* done, RpcControllerPtr cnt)
{
    --_wait_count;
    if(cnt->IsSync())
    {
        cnt->Signal();
    }
    else
    {
        CHECK(done);
        _client_ptr->GetCallBackGroup()->Post(done);
    }
}

void RpcSimpleChannel::MockDoneCallBack(RpcController* cnt)
{
    std::string s = cnt->Failed() ? "Failed" : "Success";
    cnt->Done("MockDoneCallback succeed", cnt->Failed());
    LOG(DEBUG, "MockDoneCallBack(): mock callback: [%s] reason:[%s]", s.c_str(), cnt->ErrorText().c_str());
}

}
