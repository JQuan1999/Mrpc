#include<mrpc/common/logger.h>

namespace mrpc{

LogLevel Logger::_level = LOG_LEVEL_ERROR;

Logger Logger::logger;

Logger::Logger()
{

}

Logger::~Logger()
{
    
}

Logger* Logger::GetLogHandler()
{
    return &logger;
}

void Logger::SetLogLevel(LogLevel level)
{
    _level = level;
}

LogLevel Logger::GetLogLevel()
{
    return _level;
}

void Logger::WriteLog(LogLevel level, const char* filename, int line, const char* fmt, ...)
{
    static const char* level_names[] = {"FATAL", "ERROR", "WARNNING", "INFO", "TRACE", "DEBUG"};

    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, 1024, fmt, ap);
    struct timeval now_tv;
    gettimeofday(&now_tv, nullptr);
    const time_t seconds = now_tv.tv_sec;
    struct tm t;
    localtime_r(&seconds, &t);
    fprintf(stderr, "[mrpc %s %04d/%02d/%02d-%02d:%02d:%02d.%06d %llx %s:%d] %s\n",
            level_names[level],
            t.tm_year + 1970,
            t.tm_mon + 1,
            t.tm_mday, 
            t.tm_hour,
            t.tm_min,
            t.tm_sec,
            static_cast<int>(now_tv.tv_usec),
            static_cast<long long unsigned int>(pthread_self()),
            filename, line, buf);

    fflush(stderr);

    if(level == LOG_LEVEL_FATAL)
    {
        abort();
    }
    va_end(ap);
}

} // namespace mrpc