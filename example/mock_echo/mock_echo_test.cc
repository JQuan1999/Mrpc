#include <mrpc/client/mrpc_client.h>
#include <mrpc/client/simple_rpc_channel.h>
#include <mrpc/common/rpc_controller.h>
#include <mrpc/client/mock_test.h>
#include <gtest/gtest.h>
#include "echo.pb.h"

using namespace mrpc;
using namespace MockEchoTest;

class EchoMockTest: public testing::Test
{
public:
    EchoMockTest()
        : client_ptr(new RpcClient())
    {

    }
    virtual ~EchoMockTest()
    {

    }
    virtual void SetUp()
    {

    }
    virtual void TearDown()
    {

    }
    RpcClientPtr client_ptr;  
};

void MockTestSuccess(google::protobuf::RpcController* controller, 
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response, 
                    google::protobuf::Closure* done)
{
    ((RpcController*)controller)->SetSuccess("call success");
    ((Request*)request)->set_request(((Response*)response)->response());
    if(done){
        done->Run();
    }
}

TEST_F(EchoMockTest, success_mock_sync_test)
{
    MockTest::GetSingleMockTest()->EnableMockTest();
    MockTest::GetSingleMockTest()->ClearMethod();

    SimpleChannelPtr channel(new RpcSimpleChannel(client_ptr, MOCK_TEST_ADDRESS, 12345));
    EchoServer_Stub stub(channel.get());

    const google::protobuf::ServiceDescriptor* svc = stub.GetDescriptor();
    const google::protobuf::MethodDescriptor* method = svc->method(0);
    std::string name = svc->name() + ":" + method->name();
    MockTest::GetSingleMockTest()->RegisterMethod(name, MockTestSuccess);

    RpcControllerPtr cnt(new RpcController());
    Request* request = new Request();
    Response* response = new Response();
    response->set_response("hello world");
    stub.Echo(cnt.get(), request, response, nullptr);
    EXPECT_EQ(cnt->Failed(), false);
    EXPECT_EQ(cnt->Reason(), "call success");
    EXPECT_EQ(request->request(), "hello world");
    
    delete request;
    delete response;
}

TEST_F(EchoMockTest, test_sync_ungister_method)
{
    MockTest::GetSingleMockTest()->EnableMockTest();
    MockTest::GetSingleMockTest()->ClearMethod();

    SimpleChannelPtr channel_ptr(new RpcSimpleChannel(client_ptr, MOCK_TEST_ADDRESS, 12345));
    EchoServer_Stub stub(channel_ptr.get());
    
    const google::protobuf::ServiceDescriptor* svc = stub.GetDescriptor();
    const google::protobuf::MethodDescriptor* method = svc->method(0);
    std::string name = svc->name() + ":" + method->name();
    std::string failed_reason = name + " is not existed";

    RpcControllerPtr cnt(new RpcController());
    Request* request = new Request();
    Response* response = new Response();
    stub.Echo(cnt.get(), request, response, nullptr);
    EXPECT_EQ(cnt->Failed(), true);
    EXPECT_EQ(cnt->Reason(), failed_reason);
    delete request;
    delete response;
}


void MockTestFailed(google::protobuf::RpcController* controller, 
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response, 
                    google::protobuf::Closure* done)
{
    std::string error_reason = "TestFailed";
    ((RpcController*)controller)->SetFailed(error_reason);
    ((Request*)request)->set_request(((Response*)response)->response());
    if(done)
    {
        done->Run();
    }
}

void CallBack(RpcControllerPtr controller, bool* call_flag)
{
    Request* request = dynamic_cast<Request*>(controller->GetRequest());
    Response* response = dynamic_cast<Response*>(controller->GetResponse());
    *call_flag = true;
    std::string text = response->response();
    if(controller->Failed())
    {
        LOG(INFO, "callmethod failed reason: %s response: %s", controller->ErrorText().c_str(), text.c_str());
    }else{
        LOG(INFO, "callmethod success response: %s", text.c_str());
    }
    delete request;
    delete response;
}

TEST_F(EchoMockTest, test_async_method)
{
    MockTest::GetSingleMockTest()->EnableMockTest();
    MockTest::GetSingleMockTest()->ClearMethod();

    SimpleChannelPtr channel_ptr(new RpcSimpleChannel(client_ptr, MOCK_TEST_ADDRESS, 12345));
    EchoServer_Stub stub(channel_ptr.get());

    const google::protobuf::ServiceDescriptor* svc = stub.GetDescriptor();
    const google::protobuf::MethodDescriptor* method = svc->method(0);
    std::string name = svc->name() + ":" + method->name();
    MockTest::GetSingleMockTest()->RegisterMethod(name, MockTestFailed);
    
    RpcControllerPtr cnt(new RpcController());
    Request* request = new Request();
    Response* response = new Response();
    cnt->SetRequest(request);
    cnt->SetResponse(response);

    request->set_request("failed mock test");
    bool call_flag = false;

    google::protobuf::Closure* done = google::protobuf::NewCallback(CallBack, cnt, &call_flag);
    stub.Echo(cnt.get(), request, response, done);
    usleep(20);
    EXPECT_EQ(cnt->Failed(), true);
    EXPECT_EQ(call_flag, true);
}

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}