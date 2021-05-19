#include <ros/ros.h>
#include <std_srvs/Trigger.h>

int main(int argc, char *argv[])
{
	boost::asio::io_context ioc;
    rosasio::Node node(ioc, "example_service_client");

	auto srv = node.service_client<std_srvs::Trigger>("/service");

    std_srvs::Trigger msg;
    srv->call(msg);

    std::cout << "success: " << (msg.response.success? true : false) << "\n";
    std::cout << "message: " << msg.response.message << "\n";

	return 0;
}
