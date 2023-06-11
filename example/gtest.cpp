#include<gtest/gtest.h>
#include<deque>
#include<iostream>

// 测试夹具
class Queue: public testing::Test
{
public:
    virtual void SetUp()
    {
        for(int i = 0; i < 5; i++)
        {
            _queue.push_back(i);
        }
    }
    virtual void TearDown()
    {
        std::cout<<"in TearDown(): "<<std::endl;
    }
    int Sum()
    {
        int ret = 0;
        for(auto& n: _queue)
        {
            ret += n;
        }
        return ret;
    }
    int Size()
    {
        return _queue.size();
    }
private:
    std::deque<int> _queue;
};

TEST_F(Queue, Sum)
{
    int sum = 0;
    for(int i = 0; i < 5; i++){
        sum += i;
    }
    EXPECT_EQ(Sum(), sum);
}

TEST_F(Queue, Size)
{
    EXPECT_EQ(Size(), 5);
}

int main()
{
    testing::InitGoogleTest();
    RUN_ALL_TESTS();
    return 0;
}