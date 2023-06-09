#ifndef _MRPC_ENDPOINT_H
#define _MRPC_ENDPOINT_H

#include<string>
#include<boost/asio.hpp>

#include<mrpc/common/logger.h>

using namespace boost::asio::ip;

namespace mrpc
{

typedef boost::asio::io_context IoContext;

extern std::string EndPointToString(const tcp::endpoint& endpoint);

extern bool ResovleAddress(IoContext& ioc, const std::string& address, uint32_t port, tcp::endpoint* endpoint);

}

#endif