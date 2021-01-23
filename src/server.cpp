#include <ros/ros.h>
#include <std_srvs/Trigger.h>

bool handler(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &resp)
{
    resp.success = true;
    resp.message = "Hello!";
	return true;
}

int main(int argc, char *argv[])
{
	ros::init(argc, argv, "server");

	ros::NodeHandle nh;
	auto server = nh.advertiseService("service", handler);

	ros::spin();

	return 0;
}
