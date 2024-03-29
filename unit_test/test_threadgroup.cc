#include <mrpc/common/thread_group.h>
#include <mrpc/common/logger.h>
#include <gtest/gtest.h>

using namespace mrpc;

static int count = 0;
void InitFun()
{
    count++;
}

void EndFun()
{
    count--;
}

TEST(ThreadGroup, construtor)
{
    ThreadGroup group(4, "ThreadGroup construtor test", InitFun, EndFun);
    group.Stop();
    EXPECT_EQ(count, 0);
}


void SetTrue(bool* flag)
{
    *flag = true;
}

void SetFalse(bool* flag)
{
    *flag = false;
}

TEST(ThreadGroup, post)
{
    ThreadGroup group(4, "ThreadGroup post test", nullptr, nullptr);
    bool flag;
    group.Post(std::bind(SetTrue, &flag));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(flag, true);

    group.Post(std::bind(SetFalse, &flag));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(flag, false);

    google::protobuf::Closure* func1 = google::protobuf::NewCallback<bool*>(SetTrue, &flag);
    group.Post(func1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(flag, true);

    google::protobuf::Closure* func2 = google::protobuf::NewCallback<bool*>(SetFalse, &flag);
    group.Post(func2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(flag, false);
}

TEST(ThreadGroup, dispatch)
{
    bool flag1, flag2;
    ThreadGroup group;
    group.Dispatch(std::bind(SetTrue, &flag1));
    google::protobuf::Closure* func = google::protobuf::NewCallback<bool*>(SetFalse, &flag2);
    group.Dispatch(func);
    usleep(100);
    EXPECT_EQ(flag1, true);
    EXPECT_EQ(flag2, false);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}