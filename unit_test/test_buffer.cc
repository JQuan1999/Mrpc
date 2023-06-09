#include <mrpc/common/buffer.h>
#include <gtest/gtest.h>
using namespace mrpc;

class BufferTest: public testing::Test
{
    BufferTest()
    {

    }
    virtual ~BufferTest()
    {

    }
    virtual void SetUp()
    {

    }
    virtual void TearDown()
    {

    }
};

TEST_F(BufferTest, construtor)
{
    Buffer buffer(2);
    int capacity = BUFFER_UNIT << 2;
    
}

int main()
{
    RUN_ALL_TESTS();
}