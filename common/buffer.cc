#include<mrpc/common/buffer.h>

namespace mrpc
{

//----------------Buffer------------------------
Buffer::Buffer(int factor_size)
{
    _capacity = (BUFFER_UNIT << factor_size);
        _size = 0;
        shared_buf_ptr = std::shared_ptr<char[]>(new char[_capacity+1], 
                    [](char* data){
                        delete[] data;
                        data = nullptr;
                    }
        );
    _data = shared_buf_ptr.get();
    memset(_data, 0, _capacity+1);
}

Buffer::Buffer(const Buffer& buf)
{
    _capacity = buf._capacity;
    _size = buf._size;
    shared_buf_ptr = buf.shared_buf_ptr;
    _data = buf._data;
}

Buffer::~Buffer()
{
    _data = nullptr;
    shared_buf_ptr.reset();
}

char* Buffer::GetData()
{
    return _data;
}

char* Buffer::GetHeader()
{
    return _data + _size;
}

void Buffer::SetSize(int size)
{
    _size = size;
}

int Buffer::GetSize()
{
    return _size;
}

void Buffer::Forward(int bytes)
{
    _size += bytes;
    if(_size > _capacity){
        std::cout<<"_size: "<<_size<<" is over _capacity: "<<_capacity<<", have truncted to _capacity"<<std::endl;
        _size = _capacity;
    }
}

void Buffer::Back(int bytes)
{
    _size -= bytes;
    if(_size < 0){
        std::cout<<"_size must be greater than 0"<<std::endl;
        _size = 0;
    }
}

void Buffer::SetCapacity(int cap)
{
    if(cap > _capacity)
    {
        std::cout<<"new cap must less than old capacity"<<std::endl;
        return;
    }
    _capacity = cap;
    if(_size > _capacity)
    {
        _size = _capacity;
    }
}

int Buffer::GetCapacity()
{
    return _capacity;
}

int Buffer::GetSpace()
{
    return _capacity - _size;
}

std::ostream& operator<<(std::ostream& os, const Buffer& buf)
{
    os.write(buf._data+buf._size, buf._capacity-buf._size);
    return os;
}
//----------------Buffer------------------------


//----------------ReadBuffer------------------------
ReadBuffer::ReadBuffer()
    : _read_bytes(0)
    , _last_bytes(0)
    , _total_bytes(0)
{
    _cur_iter = _buf_list.begin();
}

ReadBuffer::~ReadBuffer()
{

}

void ReadBuffer::Append(Buffer& buf)
{
    if(buf.GetCapacity() == 0)
    {
        return;
    }
    _buf_list.push_back(buf);
    _total_bytes += buf.GetCapacity();
    _cur_iter = _buf_list.begin();
}

std::string ReadBuffer::ToString()
{
    std::string ret;
    ret.reserve(_total_bytes);
    for(auto iter = _buf_list.begin(); iter != _buf_list.end(); iter++)
    {
        ret.append(iter->GetData(), iter->GetSize());
    }
    return ret;
}

ReadBufferPtr ReadBuffer::Split(int bytes)
{
    if(bytes < 0 || bytes > _total_bytes){
        LOG(FATAL, "ReadBuffer::Split() split bytes:%d must >= 0 and <= _total_bytes", bytes);
        return nullptr;
    }
    ReadBufferPtr sub = std::make_shared<ReadBuffer>();
    while(bytes > 0)
    {
        auto front = _buf_list.front();
        _buf_list.pop_front();
        int capacity = front.GetCapacity();
        if(capacity > bytes)
        {
            Buffer split(front);
            split.SetCapacity(bytes); // 前半部分的容量为bytes
            front.Forward(bytes); // 后半部分的已读字节设置为bytes即偏移量为bytes
            sub->Append(split);
            _buf_list.push_front(front); // 后半部分重新加入队列首部
            capacity = bytes;
        }else
        {
            sub->Append(front);
        }
        bytes -= capacity;
    }
    return sub;
}

bool ReadBuffer::Next(const void** data, int* size)
{
    if(_cur_iter == _buf_list.end())
    {
        _last_bytes = 0;
        return false;
    }else
    {
        *data = _cur_iter->GetHeader(); // 读取的指针
        *size = _cur_iter->GetSpace(); // 可读的大小
        _cur_iter++;
        _last_bytes = *size;
        _read_bytes += _last_bytes;
        return true;
    }
}

void ReadBuffer::BackUp(int count)
{
    if(_last_bytes <= 0)
    {
        std::cout<<"last_bytes is not greater than zero"<<std::endl;
        return;
    }
    if(count < 0)
    {
        std::cout<<"count is not greater than zero"<<std::endl;
        return;
    }
    _cur_iter--;
    _cur_iter->Back(count);
    _last_bytes = 0;
    _read_bytes -= count;
}

// 跳过指定数量的字节
bool ReadBuffer::Skip(int count)
{
    if(count < 0)
    {
        std::cout<<"count is not greater than zero"<<std::endl;
        return false;
    }
    const void* data;
    int size;
    while(count > 0 && Next(&data, &size))
    {
        if(size > count)
        {
            BackUp(size - count);
            size = count;
        }
        count -= size;
    }
    _read_bytes += count;
    _last_bytes = 0;
    return count == 0;
}

std::ostream& operator<<(std::ostream& os, ReadBuffer& buf)
{
    for(auto iter = buf._buf_list.begin(); iter != buf._buf_list.end(); iter++)
    {
        os<<*iter;
    }
    return os;
}

int64_t ReadBuffer::ByteCount() const
{
    return _read_bytes;
}
//----------------ReadBuffer------------------------


//----------------writeBuffer------------------------
WriteBuffer::WriteBuffer()
    : _last_bytes(0)
    , _total_bytes(0)
    , _write_bytes(0)
{
    _cur_iter = _buf_list.rend();
}

void WriteBuffer::SwapOut(ReadBuffer* readbuf)
{
    while(!_buf_list.empty())
    {
        Buffer buf = _buf_list.front();
        _buf_list.pop_front();
        buf.SetCapacity(buf.GetSize()); // 已写入的size为可读的容量
        buf.SetSize(0); // 已读的size = 0
        readbuf->Append(buf);
    }
    _last_bytes = 0;
    _total_bytes = 0;
    _write_bytes = 0;
}

std::string WriteBuffer::ToString()
{
    std::string ret;
    ret.reserve(_total_bytes);
    for(auto iter = _buf_list.begin(); iter != _buf_list.end(); iter++)
    {
        ret.append(iter->GetData(), iter->GetSize());
    }
    return ret;
}

bool WriteBuffer::Next(void** data, int* size)
{
    if(_cur_iter == _buf_list.rend() || _cur_iter->GetSpace() == 0)
    {
        if(!Extend())
        {
            _last_bytes = 0;
            return false;
        }
    }
    *data = _cur_iter->GetHeader();
    *size = _cur_iter->GetSpace();
    _cur_iter->Forward(*size);
    _cur_iter++;
    _last_bytes = *size;
    _write_bytes += _last_bytes;
    return true;
}

void WriteBuffer::BackUp(int count)
{
    if(_last_bytes <= 0)
    {
        std::cout<<"last_bytes is not greater than zero"<<std::endl;
        return;
    }
    if(count < 0)
    {
        std::cout<<"count is not greater than zero"<<std::endl;
        return;
    }
    --_cur_iter;
    _cur_iter->Back(count);
    _last_bytes = 0;
    _write_bytes -= count;
}

int64_t WriteBuffer::ByteCount() const
{
    return _write_bytes;
}

bool WriteBuffer::Extend()
{
    int factor_size = std::min((int)_buf_list.size()+BASE_FACTOR_SIZE, MAX_FACTOR_SIZE);
    _buf_list.push_back(Buffer(factor_size));
    _cur_iter = _buf_list.rbegin();
    _total_bytes += _cur_iter->GetCapacity();
    return true;
}

int WriteBuffer::TotalBytes() const
{
    return _total_bytes;
}

int WriteBuffer::BlockCount() const
{
    return _buf_list.size();
}
}
//----------------writeBuffer------------------------