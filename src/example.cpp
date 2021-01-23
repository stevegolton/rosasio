#include <uvros/node.hpp>
#include <std_srvs/Trigger.h>

int main(void)
{
    uvros::Node node("dave");
    auto srv = node.service_client<std_srvs::Trigger>("/service");

    // int counter = 0;
    // while (true)
    // {
    std_srvs::Trigger msg;
    // msg.request
    srv->call(msg);
        // std::cout << "Called serv" << counter++ << "\n";
    // }

    return 0;
}
