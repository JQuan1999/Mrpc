#include<mrpc/common/end_point.h>

namespace mrpc{

std::string EndPointToString(const tcp::endpoint& endpoint)
{
    std::string address = endpoint.address().to_string();
    std::string port = std::to_string(endpoint.port());
    return address + ":" + port;
}

bool ResovleAddress(IoContext& ioc, const std::string& address, uint32_t port, tcp::endpoint* endpoint)
{
    tcp::resolver solver(ioc);
    boost::system::error_code ec;
    tcp::resolver::iterator iter = solver.resolve(tcp::resolver::query(address, std::to_string(port)), ec), end; // end为默认iterator
    if(iter != end)
    {
        *endpoint = iter->endpoint();
        return true;
    }
    else
    {
        LOG(ERROR, "ResovleAddress(): resolve address [%s: %d] failed: %s", address.c_str(), port, ec.message().c_str());
        return false;
    }
}

}