#include <mrpc/common/end_point.h>
#include <gtest/gtest.h>
#include <boost/asio.hpp>

TEST(EndpointTest, tostring)
{
    tcp::endpoint endpoint1(boost::asio::ip::address::from_string("127.0.0.1"), 12345);
    EXPECT_EQ(mrpc::EndPointToString(endpoint1), "127.0.0.1:12345");

    tcp::endpoint endpoint2(tcp::v4(), 12345); // expect "0.0.0.0:12345" 
    EXPECT_EQ(mrpc::EndPointToString(endpoint2), "0.0.0.0:12345");
}

TEST(EndpointTest, resolve)
{
    tcp::endpoint endpoint1, endpoint2;
    boost::asio::io_context ioc;
    EXPECT_EQ(mrpc::ResovleAddress(ioc, "127.0.0.1", 12345, &endpoint1), true);
    EXPECT_EQ(mrpc::EndPointToString(endpoint1), "127.0.0.1:12345");

    EXPECT_EQ(mrpc::ResovleAddress(ioc, "localhost", 12345, &endpoint2), true);
    EXPECT_EQ(mrpc::EndPointToString(endpoint2), "127.0.0.1:12345");
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}