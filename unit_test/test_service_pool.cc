#include <mrpc/server/service_pool.h>
#include <gtest/gtest.h>
#include <iostream>
#include "test_buffer.pb.h"

using namespace TestProto;

class ServiceImpl: public UserService
{
public:
    virtual void Login(::google::protobuf::RpcController* controller,
                       const ::TestProto::LoginRequest* request,
                       ::TestProto::LoginResponse* response,
                       ::google::protobuf::Closure* done)
    {
        std::string count = request->count(), password = request->password();
        std::cout <<" count = "<<count<<" password = "<<password<<std::endl;
        response->set_failed(false);
        response->set_result("login success");
        if(done){
            done->Run();
        }
    }
    virtual void Add(::google::protobuf::RpcController* controller,
                    const ::TestProto::AddRequest* request,
                    ::TestProto::AddResponse* response,
                    ::google::protobuf::Closure* done)
    {
        int a = request->a(), b = request->b();
        std::cout<<"a = "<<a<<" b = "<<b<<std::endl;
        response->set_result(a+b);
        if(done){
            done->Run();
        }
    }
};

class ServicePoolTest: public testing::Test
{
public:
    ServicePoolTest(){

    };
    virtual ~ServicePoolTest(){

    }
    virtual void SetUp()
    {

    }
    virtual void TearDown()
    {

    }
};


TEST_F(ServicePoolTest, register)
{
    ServiceImpl* service(new ServiceImpl());
    {
        mrpc::ServicePool service_pool;
        EXPECT_EQ(service_pool.RegisterService(service, true), true);
        EXPECT_EQ(service_pool.RegisterService(service, true), false);
    }
}

void LoginCallback(LoginResponse* response)
{
    std::cout<<"LoginCallback(): response: "<<response->result()<<std::endl;
}

TEST_F(ServicePoolTest, call_login)
{
    ServiceImpl* service(new ServiceImpl());
    {
        mrpc::ServicePool service_pool;
        EXPECT_EQ(service_pool.RegisterService(service, true), true);

        mrpc::ServiceBoard* svc_borad = service_pool.GetServiceBoard("UserService");
        ASSERT_NE(svc_borad, nullptr);
        google::protobuf::Service* svc = svc_borad->GetService();
        EXPECT_EQ(svc, service);
        EXPECT_EQ(svc_borad->ServiceName(), "UserService");

        mrpc::MethodBorad* method_board = service_pool.GetMethodBoard("UserService", "Login");
        ASSERT_NE(method_board, nullptr);
        EXPECT_EQ(method_board->MethodName(), "Login");
        const google::protobuf::MethodDescriptor* login_method = method_board->GetDescriptor();

        mrpc::MethodBorad* method_board2 = service_pool.GetMethodBoard("UserService", "Login2");
        EXPECT_EQ(method_board2, nullptr);
        LoginRequest* request = new LoginRequest();
        request->set_count("jq");
        request->set_password("123456");
        LoginResponse* response = new LoginResponse();
        google::protobuf::Closure* login_done = google::protobuf::NewCallback(LoginCallback, response);

        service->CallMethod(login_method, nullptr, request, response, login_done);
        delete request;
        delete response;
    }
}

void AddCallBack(AddResponse* response)
{
    std::cout<<"LoginCallback(): response: "<<response->result()<<std::endl;
}

TEST_F(ServicePoolTest, call_add)
{
    ServiceImpl* service(new ServiceImpl());
    {
        mrpc::ServicePool service_pool;
        EXPECT_EQ(service_pool.RegisterService(service, true), true);

        mrpc::ServiceBoard* svc_borad = service_pool.GetServiceBoard("UserService");
        ASSERT_NE(svc_borad, nullptr);
        google::protobuf::Service* svc = svc_borad->GetService();
        EXPECT_EQ(svc, service);

        mrpc::MethodBorad* method_board = service_pool.GetMethodBoard("UserService", "Add");
        ASSERT_NE(method_board, nullptr);
        EXPECT_EQ(method_board->MethodName(), "Add");
        const google::protobuf::MethodDescriptor* add_method = method_board->GetDescriptor();

        AddRequest* request = new AddRequest();
        request->set_a(1);
        request->set_b(3);
        AddResponse* response = new AddResponse();
        google::protobuf::Closure* add_done = google::protobuf::NewCallback(AddCallBack, response);

        service->CallMethod(add_method, nullptr, request, response, add_done);
        delete request;
        delete response;
    }
}

int main()
{
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}