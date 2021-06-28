#include <iostream>
#include <rosasio/node.hpp>
#include <rosasio/subscriber.hpp>
#include <rosasio/timer.hpp>

#include <std_msgs/String.h>

int main(void)
{
    rosasio::Exec exec;
    rosasio::Node node(exec.get_ioc(), "/perf_subscriber");

    std::size_t counter = 0;

    auto sub = std::make_shared<rosasio::Subscriber<std_msgs::String>>(
        node,
        "/topic",
        [&counter](const std_msgs::String &) {
            ++counter;
        });

    auto timer = std::make_shared<rosasio::Timer>(exec.get_ioc(), std::chrono::seconds(1), [&counter]() {
        std::cout << "Received " << counter << " messages\n";
        counter = 0;
    });

    std::cout << "Subscribed to /topic, spinning forever...\n";

    exec.run();

    return 0;
}
