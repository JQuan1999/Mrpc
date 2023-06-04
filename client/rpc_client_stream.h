#ifndef _MRPC_CLIENT_STREAM_H
#define _MRPC_CLIENT_STREAM_H

#include<memory>
#include<map>
#include<atomic>
#include<mutex>

#include<mrpc/controller/rpc_controller.h>
#include<mrpc/common/end_point.h>
#include<mrpc/common/rpc_byte_stream.h>

namespace mrpc{

class RpcClientStream;
typedef std::shared_ptr<RpcClientStream> RpcClientStreamPtr;

class RpcClientStream: public RpcByteStream{
public:
    RpcClientStream(IoContext& ioc, const tcp::endpoint& endpoint)
        : _send_free(SEND_FREE)
        , _send_lock(SEND_LOCK)
        , RpcByteStream(ioc, endpoint)
    {
        _send_status.store(_send_free);
    }

    ~RpcClientStream()
    {
        for(auto iter = _controller_map.begin(); iter != _controller_map.end(); iter++)
        {
            iter->second->Done("RpcClientStream destructed", true);
        }
    }

    void PutIntoQueue(const RpcControllerPtr& crt)
    {
        std::lock_guard<std::mutex> _lock(_request_queue_mutex);
        _request_queue.push_back(crt);
    }

    bool GetFromQueue(RpcControllerPtr& msg)
    {
        std::lock_guard<std::mutex> _lock(_request_queue_mutex);
        if(_request_queue.empty()) return false;
        msg = _request_queue.front();
        _request_queue.pop_front();
        return true;
    }

    bool on_sending(const RpcControllerPtr& crt)
    {
        return crt->IsDone();
    }

    void CallMethod(const RpcControllerPtr& crt)
    {
        if(!IsConnected())
        {
            LOG(ERROR, "CallMethod(): socket is not connected: %s", EndPointToString(_remote_endpoint).c_str());
            crt->Done("socket is not connected", true);
            return;
        }
        // 将crt加入map
        AddRequest(crt);
        // 将msg加入发送队列
        PutIntoQueue(crt);
        // 开始发送
        StartSend();
    }

private:

    void AddRequest(const RpcControllerPtr& crt)
    {
        uint64_t _id = crt->GetSequenceId();
         std::lock_guard<std::mutex> lock(_controller_map_mutex);
        _controller_map[_id] = crt;
    }

    void EraseRequest(int sequence_id)
    {
        std::lock_guard<std::mutex> lock(_controller_map_mutex);
        if(_controller_map.count(sequence_id))
        {
            _controller_map.erase(sequence_id);
        }
    }

    void StartSend()
    {
        if(IsConnected() && _send_status.compare_exchange_strong(_send_free, _send_lock))
        {
            if(GetFromQueue(_send_crt))  // 从队列中取出待发送的crt和msg
            {
                if(on_sending(_send_crt)) // 发送时检查是否超时 已经完成
                {
                    LOG(INFO, "StartSend(): the rpc request has been done may been timeout: %s", EndPointToString(_remote_endpoint));
                    _send_status.compare_exchange_strong(_send_lock, _send_free);
                    ClearSendEnv();
                    return;
                }
                else
                {
                    // 获取要发送的数据并发送
                    _send_message = _send_crt->GetRequestMessage();
                    AsnycWrite(&_send_message[0], _send_message.size());
                }
            }
            else
            {
                // 队列为空
                _send_status.compare_exchange_strong(_send_lock, _send_free);
                return;
            }
        }
    }

    // AsnycWrite的回调函数
    void OnWriteSome(const boost::system::error_code& ec, size_t bytes)
    {
        if(ec)
        {
            LOG(ERROR, "OnWriteSome(): %s: write erorr: %s", EndPointToString(_remote_endpoint).c_str(), ec.message().c_str());
            // 调用当前crt的done函数
            _send_crt->Done("write erorr", true);
            EraseRequest(_send_crt->GetSequenceId());
            ClearSendEnv();
            // 关闭连接
            Close(ec.message());
            return;
        }
        if(bytes == _send_message.size()){
            LOG(INFO, "send to bytes data to %s succeed", EndPointToString(_remote_endpoint).c_str());
            _send_status.compare_exchange_strong(_send_lock, _send_free); // 在回调函数中恢复_send_free
            ClearSendEnv();
            StartSend(); // 继续尝试发送
        }
        else
        {
            // 数据未全部发送 继续调用AsnycWrite进行发送
            _send_message.erase(0, bytes);
            AsnycWrite(&_send_message[0], _send_message.size());
        }
    }

    // AsyncRead的回调函数
    void OnReadSome(const boost::system::error_code& ec, size_t bytes)
    {
        _send_crt->Done("write erorr", true);
        EraseRequest(_send_crt->GetSequenceId());
    }

    // 重置发送的临时变量
    void ClearSendEnv()
    {
        _send_crt.reset();
        _send_message.clear();
    }

    // 重置接收的临时变量
    void ClearReceiveEnv()
    {
        _receive_message.clear();
    }

private:
    enum SEND_STATUS{
        SEND_LOCK = 0,
        SEND_FREE = 1
    };
    SEND_STATUS _send_free;
    SEND_STATUS _send_lock;
    std::atomic<SEND_STATUS> _send_status;

    std::deque<RpcControllerPtr> _request_queue;
    RpcControllerPtr _send_crt; // 当前待发送的crt
    std::string _send_message; //当前待发送的msg
    std::string _receive_message; // 当前接收的msg

    
    std::mutex _request_queue_mutex;
    std::mutex _controller_map_mutex;
    std::map<uint64_t, RpcControllerPtr> _controller_map; // sequence_id -> controller
};
}

#endif