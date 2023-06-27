#ifndef _MRPC_THREAD_GROUP_H
#define _MRPC_THREAD_GROUP_H

#include<boost/asio.hpp>
#include<vector>
#include<thread>
#include<google/protobuf/stubs/callback.h>

#include<mrpc/common/logger.h>
#include<mrpc/common/end_point.h>

namespace mrpc{

class ThreadGroup;
typedef std::shared_ptr<ThreadGroup> ThreadGroupPtr;

typedef std::function<void()> ThreadFunc;
typedef void(*FuncType)();

struct ThreadParam
{
    int id; // 线程id
    FuncType init_func; // 线程初始化函数
    FuncType end_func; // 线程结束函数
    IoContext& ioc;
    ThreadParam(int p_id, FuncType p_init_func, FuncType p_end_func, IoContext& p_ioc)
        : id(p_id)
        , init_func(p_init_func)
        , end_func(p_end_func)
        , ioc(p_ioc)
        {}
    ~ThreadParam(){}
};

class ThreadGroup
{
public:
    ThreadGroup(int thread_num = 2, std::string name = "", FuncType init_func = nullptr, FuncType end_func = nullptr);

    ~ThreadGroup();
    
    void Start();

    void Stop();

    void Post(ThreadFunc task);

    void Post(google::protobuf::Closure* handle);

    void Dispatch(ThreadFunc post);

    void Dispatch(google::protobuf::Closure* handle);

    IoContext& GetService();

private:
    ThreadGroup(const ThreadGroup&);
    ThreadGroup& operator=(const ThreadGroup&);

    // 将google::protobuf::Closure*绑定为ioc可调用的函数
    static void CallbackHelper(google::protobuf::Closure* task);

    static void ThreadRun(ThreadParam param);
    
    int _thread_num; // thread num
    std::string _name;
    bool _is_running; // 是否运行

    boost::asio::io_context _ioc;
    boost::asio::io_context::work _work;
    std::vector<std::thread> _threads;
    FuncType _init_func;
    FuncType _end_func;
};

} // namespace mrpc

#endif
