#ifndef _MRPC_FUNCTION_TRACER_H_
#define _MRPC_FUNCTION_TRACER_H_

#define ENABLE_FUNCTION_TRACE
#include<mrpc/common/common.h>

namespace mrpc
{

class FuncTracer
{
public:
    FuncTracer(const char* file, size_t line, const char* func)
        : _file(file)
        , _line(line)
        , _func(func)
    {
        LOG(TRACE, "%s: %u: >%s()", _file, _line, _func);
    }

    ~FuncTracer()
    {
        LOG(TRACE, "%s: %u: <%s()", _file, _line, _func);
    }

private:
    const char* _file;
    size_t _line;
    const char* _func;
};

#ifdef ENABLE_FUNCTION_TRACE
#define FUNCTION_TRACE \
    FuncTracer __function__tracer(__FILE__, __LINE__, __FUNCTION__)
#endif

} // namespace name

#endif