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

class ThreadGroup: public boost::noncopyable{
public:
    struct ThreadParam{
        int id;
        ThreadFunc init_func;
        ThreadFunc end_func;
        IoContext& ioc;
        ThreadParam(int p_id, ThreadFunc p_init_func, ThreadFunc p_end_func, IoContext& p_ioc)
            : id(p_id)
            , init_func(p_init_func)
            , end_func(p_end_func)
            , ioc(p_ioc)
            {}
        ~ThreadParam(){}
    };

    ThreadGroup(int thread_num = 2, const std::string& name = "", ThreadFunc init_func = nullptr, ThreadFunc end_func = nullptr);
    ~ThreadGroup();
    void Start();
    void Stop();

    void post(ThreadFunc task){
        _ioc.post(task);
    }

    void post(google::protobuf::Closure* handle){
        ThreadFunc task = std::bind(&ThreadGroup::_callback_helper, handle);
        _ioc.post(task);
    }

    void dispatch(ThreadFunc post){
        _ioc.dispatch(post);
    }

    void dispatch(google::protobuf::Closure* handle){
        ThreadFunc task = std::bind(&ThreadGroup::_callback_helper, handle);
        _ioc.dispatch(task);
    }

    IoContext& GetService();

private:
    // 将google::protobuf::Closure*绑定为ioc可调用的函数
    static void _callback_helper(google::protobuf::Closure* task){
        task->Run();
    }

    static void _thread_run(ThreadParam param);
    
    int _thread_num; // thread num
    std::string _name;
    bool _is_running; // 是否运行

    boost::asio::io_context _ioc;
    boost::asio::io_context::work _work;
    std::vector<std::thread> _threads;
    ThreadFunc _init_func;
    ThreadFunc _end_func;
};

} // namespace mrpc

#endif
