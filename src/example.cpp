#include <rosasio/node.hpp>
#include <std_srvs/Trigger.h>
#include <std_msgs/String.h>

int main(void)
{
    // boost::asio::io_context ioc;
    
    // ioc.run();
    // exit(0);

    rosasio::Node node("dave");
    // auto srv = node.service_client<std_srvs::Trigger>("/service");

    // std_srvs::Trigger msg;
    // srv->call(msg);

    // std::cout << "success: " << (msg.response.success? true : false) << "\n";
    // std::cout << "message: " << msg.response.message << "\n";

    // auto sub = node.subscribe<std_msgs::String>("/topic", [](const std_msgs::String &msg) {
    //     std::cout << "Got msg: " << msg.data << "\n";
    // });

    auto pub = node.advertise<std_msgs::String>("/topic");

    int counter = 0;

    auto timer = node.create_timer(std::chrono::milliseconds(1000), [&pub, &counter]() {
        std::cout << "Timer tick\n";

        std_msgs::String msg;
        msg.data = "hi + " + std::to_string(counter++);
        pub->publish(msg);
    });

    // std_srvs::Trigger msg;
    // srv->call(msg);

    // std::cout << "success: " << (msg.response.success? true : false) << "\n";
    // std::cout << "message: " << msg.response.message << "\n";

    node.spin();

    return 0;
}
