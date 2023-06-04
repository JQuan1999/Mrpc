#ifndef MRPC_SERVICE_POOL_H
#define MRPC_SERVICE_POOL_H

#include<mrpc/common/logger.h>

#include<unordered_map>
#include<string>
#include<google/protobuf/service.h>
#include<google/protobuf/descriptor.h>

namespace mrpc
{
class ServicePool;
typedef std::shared_ptr<ServicePool> ServicePoolPtr;

class ServicePool
{
public:
    ServicePool()
    {

    }
    ~ServicePool()
    {
        for(auto& service: _service_board)
        {
            delete service.second;
        }
    }

    ServicePool(const ServicePool&) =delete;
    ServicePool& operator=(const ServicePool&) =delete;

    bool RegisterService(google::protobuf::Service* svc, bool ownership = true)
    {
        if(svc == nullptr)
        {
            LOG(FATAL, "ServicePool::RegisterService(): register serivce is nullptr");
            return false;
        }
        // 检查是否存在
        const std::string& svc_name = svc->GetDescriptor()->name();
        if(_service_board.count(svc_name))
        {
            return false;
        }
        ServiceBoard* svc_borad = new ServiceBoard(svc, ownership);
        _service_board[svc_name] = svc_borad;
        ++_count;
        return true;
    }

    ServiceBoard* GetService(const std::string& name)
    {
        if(_service_board.count(name))
        {
            return _service_board[name];
        }else{
            return nullptr;
        }
    }

    const google::protobuf::MethodDescriptor* GetMethodDescriptor(const std::string& service_name, const std::string& method_name)
    {
        ServiceBoard* svc_board = GetService(service_name);
        if(svc_board == nullptr)
        {
            return nullptr;
        }
        return svc_board->GetMethod(method_name);
    }

private:
    int _count;
    std::unordered_map<std::string, ServiceBoard*> _service_board;
};

class ServiceBoard
{
public:
    ServiceBoard()
        : _svc(nullptr)
        , _svc_descriptor(nullptr)
        , _own(true)
    {

    }
    ServiceBoard(google::protobuf::Service* _svc, bool _own)
        : _svc(_svc)
        , _svc_descriptor(_svc->GetDescriptor())
        , _own(true)
    {
        int method_count = _svc_descriptor->method_count();
        for(int i = 0; i < method_count; i++)
        {
            const google::protobuf::MethodDescriptor* des = _svc_descriptor->method(i);
            std::string method_name = des->name();
            MethodBorad* method = new MethodBorad(des);
            _method_borad[method_name] = method;
        }
    }
    ServiceBoard(const ServiceBoard&) = delete;
    ServiceBoard& operator=(const ServiceBoard&) = delete;

    ~ServiceBoard()
    {
        for(auto& method: _method_borad)
        {
            if(method.second != nullptr)
            {
                delete method.second;
            }
        }
        _method_borad.clear();
        if(_own)
        {
            delete _svc;
        }
    }

    google::protobuf::Service* GetService()
    {
        return _svc;
    }

    std::string GetName()
    {
        if(_svc_descriptor == nullptr)
        {
            LOG(FATAL, "ServiceBoard::GetName(): _svc_descriptor is nullptr");
            return "";
        }
        return _svc_descriptor->name();
    }

    const google::protobuf::MethodDescriptor* GetMethod(const std::string& name)
    {
        if(_method_borad.count(name))
        {
            return _method_borad[name]->GetDescriptor();
        }else{
            return nullptr;
        }
    }

private:
    google::protobuf::Service* _svc;
    bool _own;
    const google::protobuf::ServiceDescriptor* _svc_descriptor;
    std::unordered_map<std::string, MethodBorad*> _method_borad;
};

class MethodBorad
{
public:
    MethodBorad()
        : _method_descriptor(nullptr)
    {

    }
    MethodBorad(const google::protobuf::MethodDescriptor* des)
        : _method_descriptor(des)
    {

    }
    std::string MethodName()
    {
        return _method_descriptor->name();
    }
    const google::protobuf::MethodDescriptor* GetDescriptor()
    {
        return _method_descriptor;
    }
private:
    const google::protobuf::MethodDescriptor* _method_descriptor;
};
}
#endif