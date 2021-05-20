#include <iostream>
#include <rosasio/node.hpp>
#include <rosasio/service_client.hpp>
#include <std_srvs/Trigger.h>

int main(void)
{
    rosasio::Exec exec;
    rosasio::Node node(exec.get_ioc(), "example_service_client");

    auto service_client = std::make_shared<rosasio::ServiceClient<std_srvs::Trigger>>(node, "/service");

    std_srvs::Trigger msg;
    service_client->call(msg);

    std::cout << "success: " << msg.response.success << "\n";
    std::cout << "message: " << msg.response.message << "\n";

    return 0;
}
