#include <iostream>
#include <rosasio/node.hpp>
#include <rosasio/subscriber.hpp>

#include <std_msgs/String.h>

int main(void)
{
    rosasio::Exec exec;
    rosasio::Node node(exec.get_ioc(), "example_subscriber");

    auto sub = std::make_shared<rosasio::Subscriber<std_msgs::String>>(
        node,
        "/topic",
        [](const std_msgs::String &msg) {
            std::cout << "Got a message: " << msg.data << "\n";
        });

    std::cout << "Subscribed to /topic, spinning forever...\n";

    exec.run();

    return 0;
}
