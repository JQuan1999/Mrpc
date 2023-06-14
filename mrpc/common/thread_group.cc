#include<mrpc/common/thread_group.h>

namespace mrpc
{

ThreadGroup::ThreadGroup(int thread_num, std::string name, ThreadFunc init_func, ThreadFunc end_func)
    : _ioc()
    , _work(_ioc)
    , _thread_num(thread_num)
    , _name(name)
    , _init_func(init_func)
    , _end_func(end_func)
    , _is_running(false)
{
    if(name.size() == 0)
    {
        char tmp[20];
        sprintf(tmp, "%p", this);
        _name = tmp;
    }
    Start();
}

ThreadGroup::~ThreadGroup()
{
    Stop();
}

void ThreadGroup::Start()
{
    if(_is_running)
    {
        return;
    }
    _is_running = true;
    for(int i = 0; i < _thread_num; i++)
    {
        ThreadParam param(i+1, _init_func, _end_func, _ioc);
        _threads.emplace_back(&ThreadGroup::ThreadRun, param);
    }
    LOG(INFO, "Start(): thread group [%s] started, thread num = %d", _name.c_str(), _thread_num);
}

void ThreadGroup::Stop()
{
    if(!_is_running) return;
    _is_running = false;
    _ioc.stop();
    for(int i = 0; i < _thread_num; i++)
    {
        _threads[i].join();
    }
    LOG(INFO, "Stop(): thread group [%s] stopped", _name.c_str());
}

void ThreadGroup::Post(ThreadFunc task)
{
    _ioc.post(task);
}

void ThreadGroup::Post(google::protobuf::Closure* handle)
{
    ThreadFunc task = std::bind(&ThreadGroup::CallbackHelper, handle);
    _ioc.post(task);
}

void ThreadGroup::Dispatch(ThreadFunc task)
{
    _ioc.dispatch(task);
}

void ThreadGroup::Dispatch(google::protobuf::Closure* handle)
{
    ThreadFunc task = std::bind(&ThreadGroup::CallbackHelper, handle);
    _ioc.dispatch(task);
}

void ThreadGroup::CallbackHelper(google::protobuf::Closure* task)
{
    task->Run();
}

void ThreadGroup::ThreadRun(ThreadParam param)
{
    // init
    if(param.init_func)
    {
        param.init_func();
    }
    LOG(INFO, "ThreadRun(): thread id: [%d] is started successfully", param.id);
    // run asio
    param.ioc.run();
    // destory
    if(param.end_func)
    {
        param.end_func();
    }
}

IoContext& ThreadGroup::GetService()
{
    return _ioc;
}

} // namespace mrpc