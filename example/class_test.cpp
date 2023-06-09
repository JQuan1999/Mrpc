#include<iostream>
#include<memory>
#include<stdlib.h>
#include<unistd.h>
#include<thread>
#include<boost/asio.hpp>
using namespace std;

boost::asio::io_context ioc;
class Test
{
public:
    Test(): _id(count)
    {
        count++;
    }
    Test(const Test& t)
    {
        cout<<"copy param t address is :"<<&t<<" this address is"<<this<<endl;
    }
    ~Test()
    {
        cout<<"in ~Test(): t address is "<<this<<endl;
    }
private:
    int _id;
    static int count;
};

int Test::count = 0;

// 测试传递局部对象的引用

void CallBack(Test& t)
{
    this_thread::sleep_for(chrono::seconds(5));
    std::cout<<"after sleep 5s param t address: "<<&t<<std::endl;
}

void Fun()
{
    Test t;
    cout<<"t address is :"<<&t<<endl;
    auto func = std::bind(CallBack, t);
    func();
    // CallBack(t);
}


int main()
{
    Fun();
    // ioc.run();
    return 0;
}