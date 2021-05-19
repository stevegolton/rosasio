#include <rosasio/node.hpp>
#include <rosasio/publisher.hpp>
#include <rosasio/timer.hpp>

#include <std_srvs/Trigger.h>
#include <std_msgs/String.h>

int main(void)
{
    rosasio::Exec exec;
    rosasio::Node node(exec.get_ioc(), "example_publisher");

    auto pub = std::make_shared<rosasio::Publisher<std_msgs::String>>(node, "/topic");

    int counter = 0;

    auto timer = std::make_shared<rosasio::Timer>(exec.get_ioc(), std::chrono::seconds(1), [&pub, &counter]() {
        std::cout << "Timer tick\n";

        std_msgs::String msg;
        msg.data = "hi + " + std::to_string(counter++);
        pub->publish(msg);
    });

    exec.run();

    return 0;
}
