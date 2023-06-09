#ifndef _MRPC_BUFFER_H
#define _MRPC_BUFFER_H
#include<memory>
#include<deque>
#include<functional>
#include<google/protobuf/io/zero_copy_stream.h>

#include<mrpc/common/logger.h>
namespace mrpc
{
#define BUFFER_UNIT 64
#define BASE_FACTOR_SIZE 3
#define MAX_FACTOR_SIZE 9

class ReadBuffer;
typedef std::shared_ptr<ReadBuffer> ReadBufferPtr;

class Buffer{

public:
    Buffer(int factor_size);
    Buffer(const Buffer& buf);
    ~Buffer();
    char* GetData();

    char* GetHeader();

    bool SetSize(int size);

    int GetSize();

    bool Forward(int bytes);

    bool Back(int bytes);

    bool SetCapacity(int cap);

    int GetCapacity();

    int GetSpace();

    friend std::ostream& operator<<(std::ostream& os, const Buffer& buf);

private:
    char* _data;
    std::shared_ptr<char[]> shared_buf_ptr;
    int _capacity;
    int _size;
};

class ReadBuffer: public google::protobuf::io::ZeroCopyInputStream
{
public:
    ReadBuffer();
    ~ReadBuffer();
    void Append(Buffer& buf);
    std::string ToString();
    ReadBufferPtr Split(int bytes);

    // 继承ZeroCopyOutputStream的方法--------
    // 返回一段可读的连续内存及大小 内存buffer大小为*size，*data指向了这段内存
    bool Next(const void** data, int* size); 
    // The last "count" bytes of the last buffer returned by Next() will be
    // pushed back into the stream.
    // 针向后移动count字节 归还上次Next()执行的多余字节
    void BackUp(int count);
    // 跳过count字节
    bool Skip(int count);
    // 读取的字节总数
    int64_t ByteCount() const;
    // --------继承ZeroCopyOutputStream的方法

    friend std::ostream& operator<<(std::ostream& os, ReadBuffer& buf);
private:
    std::deque<Buffer> _buf_list;
    std::deque<Buffer>::iterator _cur_iter;
    int _last_bytes; // 最后一次读取的字节数
    int _total_bytes; // 累计可读的字节数
    int64_t _read_bytes; // 已读的字节数
};


class WriteBuffer: public google::protobuf::io::ZeroCopyOutputStream
{
public:
    WriteBuffer();
    ~WriteBuffer();
    void SwapOut(ReadBuffer* readbuf);
    std::string ToString();
    int64_t Reserve(int bytes);
    void SetData(int head, const char* data, int bytes);
    // 继承ZeroCopyOutputStream的方法--------
    // 返回一段可写的连续内存及大小 内存buffer大小为*size，*data指向了这段内存
    bool Next(void** data, int* size); 
    //归还Next申请的部分内存
    void BackUp(int count);
    // 写入的字节总数
    int64_t ByteCount() const;
    // --------继承ZeroCopyOutputStream的方法

    bool Extend(); // 分配新的buffer
    int TotalBytes() const;
    int BlockCount() const;

private:
    std::deque<Buffer> _buf_list;
    std::deque<Buffer>::reverse_iterator _cur_iter;
    int _last_bytes;
    int _total_bytes;
    int64_t _write_bytes;
};

}

#endif