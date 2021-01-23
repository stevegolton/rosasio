#include <uvros/node.hpp>
#include <std_srvs/Trigger.h>

int main(void)
{
    uvros::Node node("dave");
    auto srv = node.service_client<std_srvs::Trigger>("/service");

    std_srvs::Trigger msg;
    srv->call(msg);

    std::cout << "success: " << (msg.response.success? true : false) << "\n";
    std::cout << "message: " << msg.response.message << "\n";

    return 0;
}
