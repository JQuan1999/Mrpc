#include<mrpc/common/logger.h>

bool fun(int a, int b)
{
    return a + b == 2;
}

int main()
{
    MRPC_SET_LOG_LEVEL(DEBUG);
    mrpc::Logger* handle = mrpc::Logger::GetLogHandler();
    LOG(DEBUG, "the is a test");
    int a = 1, b = 1;
    LOG_IF(fun(a, b), INFO, "a + b = 2  a = %d, b = %d", a, b);
    b = 2;
    LOG_IF(fun(a, b), INFO, "a + b = 2  a = %d, b = %d", a, b);

    CHECK(fun(a, b));
    return 0;
}