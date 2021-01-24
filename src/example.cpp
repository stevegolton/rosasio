#include <uvros/node.hpp>
#include <std_srvs/Trigger.h>
#include <std_msgs/String.h>

int main(void)
{
    uvros::Node node("dave");
    // auto srv = node.service_client<std_srvs::Trigger>("/service");

    // std_srvs::Trigger msg;
    // srv->call(msg);

    // std::cout << "success: " << (msg.response.success? true : false) << "\n";
    // std::cout << "message: " << msg.response.message << "\n";

    auto sub = node.subscribe<std_msgs::String>("/topic", [] (const std_msgs::String &msg) {
        std::cout << "Got msg: " << msg.data << "\n";
    });

    // std_srvs::Trigger msg;
    // srv->call(msg);

    // std::cout << "success: " << (msg.response.success? true : false) << "\n";
    // std::cout << "message: " << msg.response.message << "\n";

    node.spin();

    return 0;
}
