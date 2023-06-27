#ifndef TIMEOUT_MANAGER_H
#define TIMEOUT_MANAGER_H
#include <atomic>
#include <time.h>
#include <queue>
#include <functional>
#include <mutex>
#include <boost/asio.hpp>
#include <mrpc/common/logger.h>
#include <mrpc/common/rpc_controller.h>

#define SEC_TO_USEC 1000000
#define ROUTE_TIME 1000 // 定时器的工作周期 以微秒为单位

namespace mrpc
{
class TimeoutManager;
typedef std::shared_ptr<TimeoutManager> TimeoutManagerPtr;

class TimeoutManager
{
public:
    TimeoutManager(boost::asio::io_context& ioc)
        : _ioc(ioc)
        , _timer(_ioc)
        , _is_running(false)
        , _route_time(ROUTE_TIME)
    {}
    TimeoutManager(const TimeoutManager&)=delete;

    TimeoutManager& operator=(const TimeoutManager&)=delete;
    
    ~TimeoutManager()
    {
        Stop();
    }

    long long CurrentTime()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        long long cur = tv.tv_sec * SEC_TO_USEC + tv.tv_usec;
        return cur;
    }

    void Stop()
    {
        if(!_is_running)
        {
            return;
        }
        _is_running = false;
        _timer.cancel();
        std::lock_guard<std::mutex> lock(_queue_mutex);
        while(!_cnt_queue.empty())
        {
            _cnt_queue.pop();
        }
    }

    void Add(const RpcControllerPtr& cnt)
    {
        if(!_is_running){
            return;
        }
        std::lock_guard<std::mutex> lock(_queue_mutex);
        long long expire_time = CurrentTime() + cnt->GetTimeout() * SEC_TO_USEC;
        _cnt_queue.push(Item(expire_time, cnt));
    }

    void Start()
    {
        if(_is_running)
        {
            return;
        }
        _is_running = true;
        _timer.expires_from_now(boost::asio::chrono::microseconds(_route_time));
        _timer.async_wait(std::bind(&TimeoutManager::OnTimeout, this, std::placeholders::_1));
    }

private:

    void OnTimeout(const boost::system::error_code& ec)
    {
        if(ec){
            LOG(ERROR, "OnTimeOut() timeout erorr msg: %s", ec.message().c_str());
            return;
        }
        if(_is_running)
        {
            // 检查是否有超时
            std::lock_guard<std::mutex> lock(_queue_mutex);
            long long cur_time = CurrentTime();
            while(!_cnt_queue.empty() && _cnt_queue.top().expire_time <= cur_time)
            {
                const RpcControllerPtr& cnt = _cnt_queue.top().cnt_;
                _cnt_queue.pop();
                if(!cnt || !cnt.get())
                {
                    LOG(FATAL, "rpc controller is null");
                    return;
                }
                if(!cnt->IsDone())
                {
                    cnt->Done("time out", true);
                }
            }
            _timer.expires_from_now(boost::asio::chrono::microseconds(_route_time));
            _timer.async_wait(std::bind(&TimeoutManager::OnTimeout, this, std::placeholders::_1));
        }
    }
private:
    struct Item{
        long long expire_time;
        RpcControllerPtr cnt_;
        Item(long long time = 0, const RpcControllerPtr& cnt = nullptr)
            : expire_time(time)
            , cnt_(cnt)
        {

        }
        bool operator<(const Item& item) const 
        {
            return this->expire_time > item.expire_time;
        }
    };

private:
    std::mutex _queue_mutex;
    std::priority_queue<Item> _cnt_queue;
    boost::asio::io_context& _ioc;
    boost::asio::steady_timer _timer;
    std::atomic<bool> _is_running;
    int _route_time;
};
}
#endif