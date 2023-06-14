#ifndef _MRPC_HEADER_H
#define _MRPC_HEADER_H
#include<stdint.h>
namespace mrpc{

#define MAGIC_STR_VALUE 1095126867u

// MAGIC_STR_VALUE 用于标记收到的消息为rpc头部
// rpc 头部用于标记meta大小和data大小
struct RpcHeader{
    union
    {
        char magic_str[4];
        uint32_t magic_str_value; 
    }; // 4bytes
    int32_t meta_size; // 4bytes
    int32_t data_size; // 4bytes
    int32_t message_size; // 4bytes message_size = meta_size + data_size
    RpcHeader(): magic_str_value(MAGIC_STR_VALUE), meta_size(0), data_size(0), message_size(0){}
    bool Check()
    {
        return magic_str_value == MAGIC_STR_VALUE;
    }
};
}

#endif