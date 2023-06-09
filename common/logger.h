#ifndef _MRPC_LOGGER_H_
#define _MRPC_LOGGER_H_

#include<cstdio>
#include<stdarg.h>
#include<time.h>
#include<sys/time.h>
#include<pthread.h>
#include<cstdlib>

namespace mrpc{

// 默认日志水平ERROR
enum LogLevel{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_NOTICE = 3,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_TRACE = 4,
    LOG_LEVEL_DEBUG = 5,
};

class Logger
{
public:
    static Logger* GetLogHandler();
    static void SetLogLevel(LogLevel level);
    static LogLevel GetLogLevel();
    static void WriteLog(LogLevel level, const char* filename, int line, const char* fmt, ...);
private:
    Logger();
    ~Logger();
    static LogLevel _level;
    static Logger logger;
};

// ##是字符串化
#define MRPC_SET_LOG_LEVEL(level) \
    ::mrpc::Logger::GetLogHandler()->SetLogLevel(::mrpc::LOG_LEVEL_##level)

// 当level小于设置的level时才会打印日志 __FILE__为当前文件 __LINE__为当前行
#define LOG(level, fmt, arg...) \
    (::mrpc::Logger::GetLogHandler()->GetLogLevel() < ::mrpc::LOG_LEVEL_##level) ? \
        (void)0 : ::mrpc::Logger::GetLogHandler()   \
        ->WriteLog(::mrpc::LOG_LEVEL_##level, __FILE__, __LINE__, fmt, ##arg)

// log_if
#define LOG_IF(condition, level, fmt, arg...) \
    !(condition) ? void(0) : ::mrpc::Logger::GetLogHandler() \
        ->WriteLog(::mrpc::LOG_LEVEL_##level, __FILE__, __LINE__, fmt, ##arg)

// check
#define CHECK(expression) \
    LOG_IF(!expression, FATAL, "Check failed")
}

#endif