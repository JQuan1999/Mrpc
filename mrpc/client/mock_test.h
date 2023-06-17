#ifndef MRPC_MOCK_TEST_H
#define MRPC_MOCK_TEST_H

#include <mutex>
#include <unordered_map>
#include <string>
#include <functional>
#include <google/protobuf/service.h>

#define MOCK_TEST_ADDRESS "/mock"
#define MOCK_TEST_OBJ MockTest::GetSingleMockTest()

namespace mrpc
{

typedef void(*MethodFunction)(google::protobuf::RpcController*,const google::protobuf::Message*,
                                google::protobuf::Message*, google::protobuf::Closure*);

class MockTest{
public:
    
    virtual ~MockTest()
    {

    }

    static MockTest* GetSingleMockTest()
    {
        static MockTest mock_obj;
        return &mock_obj;
    }

    bool IsEnableMockTest()
    {
        return _enable_mock_test == true;
    }

    void EnableMockTest()
    {
        _enable_mock_test = true;
    }

    void ClearMethod()
    {
        std::lock_guard<std::mutex> lock(_method_mutex);
        _method_map.clear();
    }

    void RegisterMethod(const std::string& name, MethodFunction method)
    {
        std::lock_guard<std::mutex> lock(_method_mutex);
        _method_map[name] = method;
    }

    MethodFunction FindMethod(std::string& name)
    {
        std::lock_guard<std::mutex> lock(_method_mutex);
        if(_method_map.count(name))
        {
            return _method_map[name];
        }
        else
        {
            return nullptr;
        }
    }

private:
    MockTest()
    {
        _enable_mock_test = false;
    }
    MockTest(const MockTest&);
    // name -> method
    // name = service_name + ":" + method_name
    std::unordered_map<std::string, MethodFunction> _method_map;
    std::mutex _method_mutex;
    bool _enable_mock_test;
};

}
#endif