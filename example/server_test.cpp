#include<mrpc/server/mrpc_server.h>
#include<mrpc/common/logger.h>
#include<iostream>

// g++ server_test.cpp ../server/mrpc_server.cc ../common/thread_group.cc -I../../ -g2
int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    mrpc::RpcServer server;
    std::cout<<"1"<<std::endl;
}