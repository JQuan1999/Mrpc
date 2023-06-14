#include <mrpc/common/buffer.h>
#include <gtest/gtest.h>
#include <time.h>
#include <string>
#include <mrpc/proto/rpc_header.h>
#include <mrpc/proto/rpc_meta.pb.h>
#include <mrpc/unit_test/test_buffer.pb.h>
using namespace mrpc;

char* RandStr(char* str, int len)
{
    for(int i = 0; i < len; i++)
    {
        if(i % 3 == 0)
        {
            str[i] = 'A' + rand() % 26;
        }
        else if(i % 3 == 1)
        {
            str[i] = 'a' + rand() % 26;
        }
        else{
            str[i] = '0' + rand() % 10;
        }
    }
    return str;
}

TEST(BufferTest, construtor)
{
    Buffer buffer1(2);
    int capacity = BUFFER_UNIT << 2;
    EXPECT_EQ(buffer1.GetCapacity(), capacity);
    EXPECT_EQ(buffer1.GetSize(), 0);
    EXPECT_EQ(buffer1.GetSpace(), capacity);
    EXPECT_EQ(buffer1.SetSize(capacity+1), false);
    EXPECT_EQ(buffer1.SetSize(capacity / 2), true);
    char* header = buffer1.GetHeader();
    EXPECT_EQ(header-buffer1.GetData(), capacity/2);

    Buffer buffer2;
    EXPECT_EQ(buffer2.GetCapacity(), 0);
    EXPECT_EQ(buffer2.GetData(), nullptr);
    EXPECT_EQ(buffer2.GetSize(), 0);
    EXPECT_EQ(buffer2.GetSpace(), 0);
    EXPECT_EQ(buffer2.SetSize(0), true);
}

TEST(BufferTest, forward)
{
    {
        Buffer buffer1(2);
        int capacity = BUFFER_UNIT << 2;
        EXPECT_EQ(buffer1.Forward(capacity), true);
        EXPECT_EQ(buffer1.Back(capacity / 2), true);
        EXPECT_EQ(buffer1.Back(capacity / 2), true);
        EXPECT_EQ(buffer1.Back(1), false);
        EXPECT_EQ(buffer1.Forward(capacity+1), false);
    }
    {
        Buffer buffer2(1);
        int capacity = BUFFER_UNIT << 1;
        int bytes = 10;
        EXPECT_EQ(buffer2.Forward(bytes), true);
        EXPECT_EQ(buffer2.GetSize(), bytes);
        EXPECT_EQ(buffer2.GetCapacity(), capacity);

        EXPECT_EQ(buffer2.Back(2 * bytes), false);
        EXPECT_EQ(buffer2.Back(bytes / 2), true);
        EXPECT_EQ(buffer2.GetSize(), bytes / 2);
        EXPECT_EQ(buffer2.GetSpace(), capacity - bytes / 2);
        
        int space = buffer2.GetSpace();
        EXPECT_EQ(buffer2.Forward(space), true);
        EXPECT_EQ(buffer2.Forward(1), false);
        EXPECT_EQ(buffer2.GetSize(), capacity);
    }
}

class ReadBufferTest: public testing::Test
{
public:
    ReadBufferTest()
    {
        block_size = 3;
        total_bytes = 0;
    }
    virtual ~ReadBufferTest()
    {
        total_bytes = 0;
    }
    virtual void SetUp()
    {
        for(int i = 0; i < block_size; i++){
            Buffer buf(0);
            int cap = buf.GetCapacity();
            char* data = buf.GetData();
            data = RandStr(data, cap);
            buffers.push_back(buf);
            strs.emplace_back(data, cap);
            total_bytes += cap;
        }
    }
    virtual void TearDown()
    {

    }
    int block_size;
    int total_bytes;
    std::vector<std::string> strs;
    std::vector<Buffer> buffers;
};

TEST_F(ReadBufferTest, append)
{
    {
        ReadBufferPtr ptr = std::make_shared<ReadBuffer>();
        std::string str;
        for(auto& buf: buffers)
        {
            ptr->Append(buf);
            str.append(buf.GetHeader(), buf.GetSpace());
        }
        EXPECT_EQ(ptr->ToString(), str);
    }
}

TEST_F(ReadBufferTest, Split)
{
    ReadBufferPtr ptr = std::make_shared<ReadBuffer>();
    
    {
        // split 0
        ReadBufferPtr split = ptr->Split(0);
        EXPECT_EQ(split->ToString(), "");
        EXPECT_EQ(split->GetTotalBytes(), 0);
    }
    {
        // split -1bytes
        ReadBufferPtr split = ptr->Split(-1);
        EXPECT_EQ(split.get(), nullptr);
    }
    {
        int bytes = 0;
        std::string s1;
        for(int i = 0; i < block_size; i++){
            ptr->Append(buffers[i]);
            bytes += buffers[i].GetCapacity();
            s1 += strs[i];
        }
        EXPECT_EQ(ptr->GetTotalBytes(), bytes);

        // split所有block
        ReadBufferPtr split = ptr->Split(bytes);
        EXPECT_EQ(split->ByteCount(), 0);
        EXPECT_EQ(split->GetTotalBytes(), bytes);
        EXPECT_EQ(split->ToString(), s1);

        // split block1
        std::string s2 = strs[0], s3 = strs[1] + strs[2];
        ReadBufferPtr split2 = split->Split(buffers[0].GetCapacity());
        EXPECT_EQ(split2->ToString(), strs[0]);
        EXPECT_EQ(split2->GetTotalBytes(), buffers[0].GetCapacity());

        EXPECT_EQ(split->ToString(), s3);
        EXPECT_EQ(split->GetTotalBytes(), bytes - buffers[0].GetCapacity());
    }
    {
        ptr->Clear();
        for(int i = 0; i < block_size; i++)
        {
            ptr->Append(buffers[i]);
        }
        // split block1 + block2的一半
        int bytes1 = buffers[0].GetCapacity(), bytes2 = buffers[1].GetCapacity();
        int bytes3 = bytes1 + bytes2 / 2, bytes4 = bytes1 + bytes2 - bytes3;
        std::string s1 = strs[0] + strs[1];
        std::string s2 = s1.substr(bytes3);
        s1.erase(bytes3);

        ReadBufferPtr split = ptr->Split(bytes3);
        EXPECT_EQ(split->GetTotalBytes(), bytes3);
        EXPECT_EQ(split->ToString(), s1);
        EXPECT_EQ(ptr->GetTotalBytes(), total_bytes - bytes3);

        ReadBufferPtr split2 = ptr->Split(bytes4); // 第2个buffer的剩余部分
        EXPECT_EQ(split2->GetTotalBytes(), bytes4);
        EXPECT_EQ(split2->ToString(), s2);
        EXPECT_EQ(ptr->GetTotalBytes(), total_bytes - bytes1 - bytes2);

        // split all
        int bytes5 = total_bytes - bytes1 - bytes2;
        ReadBufferPtr split3 = ptr->Split(bytes5);
        EXPECT_EQ(split3->GetTotalBytes(), bytes5);
        EXPECT_EQ(ptr->GetTotalBytes(), 0);
    }
}

TEST_F(ReadBufferTest, Next)
{
    ReadBufferPtr ptr(new ReadBuffer());
    const void* data;
    int size = 0;
    EXPECT_EQ(ptr->Next(&data, &size), false); // next empty
    for(int i = 0; i < block_size; i++){
        ptr->Append(buffers[i]);
    }
    EXPECT_EQ(ptr->ByteCount(), 0);

    int read_bytes = 0;
    for(int i = 0; i < block_size; i++){
        EXPECT_EQ(ptr->Next(&data, &size), true);
        EXPECT_EQ(size, buffers[i].GetSpace());
        read_bytes += size;
        EXPECT_EQ(data, buffers[i].GetData());
        auto iter = ptr->GetCurrentIter();
        if(i != block_size - 1){
            EXPECT_EQ(iter->GetHeader(), buffers[i+1].GetHeader());
            EXPECT_EQ(iter->GetSize(), buffers[i+1].GetSize());
            EXPECT_EQ(iter->GetCapacity(), buffers[i+1].GetCapacity());
        }
        EXPECT_EQ(ptr->ByteCount(), read_bytes);
    }
}

TEST_F(ReadBufferTest, BackUp)
{
    ReadBufferPtr ptr(new ReadBuffer());
    const void* data;
    int size = 0;
    {
        ptr->Append(buffers[0]);
        EXPECT_EQ(ptr->Next(&data, &size), true);
        // back up 1bytes
        ptr->BackUp(1);
        EXPECT_EQ(ptr->ByteCount(), size-1);
        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), size-1);
        EXPECT_EQ(iter->GetSpace(), 1);
    }
    {
        ptr->Clear();
        ptr->Append(buffers[0]);
        EXPECT_EQ(ptr->Next(&data, &size), true);
        // back up size bytes
        ptr->BackUp(size);
        EXPECT_EQ(ptr->ByteCount(), 0);
        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), 0);
        EXPECT_EQ(iter->GetSpace(), size);
    }
    {
        // test next and back string
        ptr->Clear();
        ptr->Append(buffers[0]);
        EXPECT_EQ(ptr->Next(&data, &size), true);
        std::string s1(buffers[0].GetHeader(), buffers[0].GetSpace());
        int len = size / 2;
        ptr->BackUp(len);
        std::string s2((char*)data, size - len);
        std::string s3 = ptr->ToString();
        std::cout<<"s1 = "<<s1<<" s2 = "<<s2<<" s3 = "<<s3<<std::endl;
        EXPECT_EQ(s2+s3, s1);
    }
}

class WriteBufferTest: public testing::Test
{
public:
    WriteBufferTest(){

    }
    virtual ~WriteBufferTest()
    {

    }
    virtual void SetUp()
    {

    }
    virtual void TearDown()
    {

    }

};


TEST_F(WriteBufferTest, Reserve)
{
    {
        WriteBufferPtr ptr(new WriteBuffer());
        EXPECT_EQ(ptr->TotalBytes(), 0);
        EXPECT_EQ(ptr->ByteCount(), 0);

        int head = ptr->Reserve(2);
        EXPECT_EQ(head, 0);
        EXPECT_EQ(ptr->ByteCount(), 2);

        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), 2);
    }
    {
        // reserve剩余4字节
        WriteBufferPtr ptr(new WriteBuffer());
        void* data;
        int size;
        EXPECT_EQ(ptr->Next(&data, &size), true);
        int total_bytes = ptr->ByteCount();

        int reserve_bytes = 4;
        ptr->BackUp(reserve_bytes);
        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), total_bytes - reserve_bytes);

        ptr->Reserve(reserve_bytes);
        EXPECT_EQ(ptr->ByteCount(), total_bytes);

        // 继续reseve4字节 新分配一个block
        ptr->Reserve(reserve_bytes);
        EXPECT_EQ(ptr->ByteCount(), total_bytes + reserve_bytes);
        iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), reserve_bytes);
    }
    {
        WriteBufferPtr ptr(new WriteBuffer());
        void* data;
        int size;
        EXPECT_EQ(ptr->Next(&data, &size), true);
        int total_bytes = ptr->ByteCount();

        int reserve_bytes = 4;
        ptr->BackUp(reserve_bytes);
        ptr->Reserve(2*reserve_bytes); // 使用剩余4字节后 分配新的block使用4字节
        EXPECT_EQ(ptr->ByteCount(), total_bytes + reserve_bytes);
        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), reserve_bytes);
        EXPECT_EQ(iter->GetSpace(), ptr->TotalBytes() - ptr->ByteCount());
    }
}


TEST_F(WriteBufferTest, SetData)
{
    {
        // set data小于一个block
        std::string s1 = "abc", s2 = "def", s3 = "ghijklmn";
        WriteBufferPtr ptr(new WriteBuffer());
        void* data;
        int size;
        ptr->Next(&data, &size);
        int total_bytes = ptr->TotalBytes(); // 总共可写的空间

        int back = size - s1.size(); // 只需要s1的空间
        ptr->BackUp(back);

        strncpy((char*)data, &s1[0], s1.size());
        EXPECT_EQ(ptr->ByteCount(), s1.size());

        int head = ptr->Reserve(s2.size());
        EXPECT_EQ(s1.size() + s2.size(), ptr->ByteCount());

        ptr->Next(&data, &size);
        EXPECT_EQ(size, total_bytes - s1.size()-s2.size());
        back = size - s3.size();
        ptr->BackUp(back);
        EXPECT_EQ(ptr->ByteCount(), s1.size()+s2.size()+s3.size());
        strncpy((char*)data, &s3[0], s3.size());

        auto iter = ptr->GetCurrentIter();
        EXPECT_EQ(iter->GetSize(), s1.size()+s2.size()+s3.size());

        ptr->SetData(head, &s2[0], s2.size());
        std::string s = ptr->ToString();
        EXPECT_EQ(s1+s2+s3, s);
    }
    {
        // set data大于一个block
        WriteBufferPtr ptr(new WriteBuffer());
        void* data;
        int size;
        ptr->Next(&data, &size);
        ptr->BackUp(size/2);
        RandStr((char*)data, size / 2);
        std::string s1 = ptr->ToString();

        int head = ptr->Reserve(size);
        EXPECT_EQ(head, size / 2);
        char cs[size];
        RandStr(cs, size);
        std::string s2(cs, size);

        ptr->SetData(head, cs, size);
        EXPECT_EQ(ptr->ByteCount(), size+size/2);
        EXPECT_EQ(ptr->ToString(), s1+s2);
    }
    {
        // set data大于多个block
        WriteBufferPtr ptr(new WriteBuffer());
        void* data;
        int size;
        ptr->Next(&data, &size);
        ptr->BackUp(size/2);
        RandStr((char*)data, size / 2);
        std::string s1 = ptr->ToString();
        
        int reserve_len = 1024;
        int head = ptr->Reserve(reserve_len);
        char cs[reserve_len];
        RandStr(cs, reserve_len);
        std::string s2(cs, reserve_len);
        ptr->SetData(head, cs, reserve_len);
        EXPECT_EQ(ptr->ByteCount(), reserve_len + size / 2);
        EXPECT_EQ(ptr->ToString(), s1+s2);
    }
}


class BufferSerializeTest: public testing::Test
{
public:
    BufferSerializeTest()
    {
        test_data.set_id(1024);
        test_data.set_name("jq");
        int text_len = 1024;
        record.set_text_len(text_len);
        char text[text_len];
        RandStr(text, text_len);
        record.set_text(text);
        test_data.mutable_record()->CopyFrom(record);
    }
    virtual ~BufferSerializeTest()
    {

    }
    virtual void SetUp()
    {

    }
    virtual void TearDown()
    {

    }
    TestProto::TestData_Record record;
    TestProto::TestData test_data;
};


TEST_F(BufferSerializeTest, serialize)
{
    WriteBufferPtr w_ptr(new WriteBuffer());
    ReadBufferPtr r_ptr(new ReadBuffer());
    mrpc::RpcHeader header;
    int header_size = sizeof(header);
    int head = w_ptr->Reserve(header_size);
    EXPECT_EQ(head, 0);

    std::string method = "method";
    std::string service = "service";
    mrpc::RpcMeta meta;
    meta.set_type(mrpc::RpcMeta_Type_REQUEST);
    meta.set_sequence_id(10);
    meta.set_service(service);
    meta.set_method(method);
    meta.set_failed(true);
    meta.set_failed("failed");

    EXPECT_EQ(meta.SerializeToZeroCopyStream(w_ptr.get()), true);
    int bytes = w_ptr->ByteCount();
    int meta_size = bytes - header_size;

    EXPECT_EQ(test_data.SerializeToZeroCopyStream(w_ptr.get()), true);
    int data_size = w_ptr->ByteCount() - header_size - meta_size;

    header.meta_size = meta_size;
    header.data_size = data_size;
    header.message_size = meta_size + data_size;
    w_ptr->SetData(head, reinterpret_cast<char*>(&header), header_size);
    w_ptr->SwapOut(r_ptr.get());
    EXPECT_EQ(r_ptr->GetTotalBytes(), header_size + meta_size + data_size);

    mrpc::RpcHeader parser_header;
    mrpc::RpcMeta parser_meta;
    TestProto::TestData parser_test_data;
    const void* data;
    int total_size = 0;
    int size;
    while(total_size < header_size)
    {
        EXPECT_EQ(r_ptr->Next(&data, &size), true);
        int res_data = header_size - total_size;
        if(size > res_data)
        {
            memcpy(reinterpret_cast<void*>(&parser_header), data, res_data);
            r_ptr->BackUp(size - res_data);
            total_size += res_data;
        }
        else
        {
            memcpy(reinterpret_cast<void*>(&parser_header), data, size);
            total_size += size;
        }
    }
    EXPECT_EQ(parser_header.meta_size, header.meta_size);
    EXPECT_EQ(parser_header.data_size, header.data_size);
    EXPECT_EQ(parser_header.message_size, header.message_size);

    ReadBufferPtr split = r_ptr->Split(parser_header.meta_size);
    EXPECT_EQ(split->GetTotalBytes(), parser_header.meta_size);
    EXPECT_EQ(parser_meta.ParseFromZeroCopyStream(split.get()), true);
    EXPECT_EQ(parser_meta.type(), meta.type());
    EXPECT_EQ(parser_meta.sequence_id(), meta.sequence_id());
    EXPECT_EQ(parser_meta.service(), meta.service());
    EXPECT_EQ(parser_meta.method(), meta.method());
    EXPECT_EQ(parser_meta.failed(), meta.failed());
    EXPECT_EQ(parser_meta.reason(), meta.reason());

    EXPECT_EQ(parser_test_data.ParseFromZeroCopyStream(r_ptr.get()), true);
    EXPECT_EQ(parser_test_data.id(), test_data.id());
    EXPECT_EQ(parser_test_data.name(), test_data.name());
    TestProto::TestData_Record parser_record = parser_test_data.record();
    EXPECT_EQ(parser_record.text_len(), record.text_len());
    EXPECT_EQ(parser_record.text(), record.text());
}

int main()
{
    srand(time(0));
    testing::InitGoogleTest();
    RUN_ALL_TESTS();
}