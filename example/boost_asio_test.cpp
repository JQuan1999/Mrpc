#include<mrpc/common/thread_group.h>
#include<boost/asio.hpp>
#include<iostream>
using namespace boost::asio::ip;


// 会话连接
class Session{
public:
    Session(boost::asio::io_context& ioc):_socket(ioc){}
    void Start(){
        memset(_data, 0, max_length);

        _socket.async_read_some(boost::asio::buffer(_data, max_length), 
        std::bind(&Session::handler_read, this, std::placeholders::_1, std::placeholders::_2));

    }

    tcp::socket& Socket(){return _socket;}

private:
    
    // 回调函数签名
    // void handler(
    // *   const boost::system::error_code& error, // Result of operation.
    // *   std::size_t bytes_transferred           // Number of bytes read.
    // * ); 
    void handler_read(const boost::system::error_code& error, size_t bytes_transferred){
        std::cout<<"handler read is called\n";
        if(!error){
            // 回调函数打印收到的data继续异步写回客户端
            std::cout<<"server receive data is "<< _data << std::endl;
            // async_write是通过一次或者多次调用async_write_some函数来实现的
            boost::asio::async_write(_socket, boost::asio::buffer(_data, bytes_transferred),
            std::bind(&Session::handler_write, this, std::placeholders::_1, std::placeholders::_2));

        }else{
            std::cout << "read error erorr value is "<<error.value() <<" msg is "<< error.message() << std::endl;
            delete this; // 删除session指针
        }
    }

    // async_write的回调函数
    void handler_write(const boost::system::error_code& error, size_t bytes_transferred){
        std::cout<<"handler write is called\n";
        if(!error){
            memset(_data, 0, max_length);
            // 继续读客户端数据
            _socket.async_read_some(boost::asio::buffer(_data, max_length), std::bind(&Session::handler_read, this, std::placeholders::_1, std::placeholders::_2));
        }else{
            std::cout << "read error erorr value is "<<error.value() <<" msg is "<< error.message() << std::endl;
            delete this; // 删除session指针
        }
    }

    tcp::socket _socket;
    enum{ max_length = 1024};
    char _data[max_length];
};


class Server{
public:
    Server(boost::asio::io_context& ioc, short port):_ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)){
        start_accept();
    }

private:
    void start_accept(){
        Session *new_session = new Session(_ioc);
        // 回调函数格式void(boost::system::error_code) 因此需要绑定new_session
        _acceptor.async_accept(new_session->Socket(), std::bind(&Server::handle_accept, this, new_session, std::placeholders::_1));
    }

    // 回调函数
    void handle_accept(Session* new_session, const boost::system::error_code& error){
        std::cout<<"handler accpet is called\n";
        if(!error){
            new_session->Start();
        }else{
            std::cout << "read error erorr value is "<<error.value() <<" msg is "<< error.message() << std::endl;
        }
        // 继续accpet
        start_accept();
    }

    boost::asio::io_context& _ioc;
    tcp::acceptor _acceptor;
};


int main()
{
    work_group.Start();
}