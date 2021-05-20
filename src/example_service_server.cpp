#include <rosasio/node.hpp>
#include <rosasio/service_server.hpp>
#include <std_srvs/Trigger.h>

int main(int argc, char *argv[])
{
	rosasio::Exec exec;
	rosasio::Node node(exec.get_ioc(), "example_service_server");

	auto server = std::make_shared<rosasio::ServiceServer<std_srvs::Trigger>>(
		node,
		"service",
		[](const std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &resp) {
			std::cout << "This service has been called\n";
			resp.success = true;
			resp.message = "Hello from the service!";
			return true;
		});

	exec.run();

	return 0;
}
