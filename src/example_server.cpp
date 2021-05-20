#include <ros/ros.h>
#include <std_srvs/Trigger.h>

bool handler(std_srvs::Trigger::Request &req, std_srvs::Trigger::Response &resp)
{
    ROS_INFO("CALLED A SERVICE!!");
    resp.success = true;
    resp.message = "Hello from the service!";
	return true;
}

int main(int argc, char *argv[])
{
	ros::init(argc, argv, "server");

	ros::NodeHandle nh;
	auto server = nh.advertiseService("service_good", handler);

	ros::spin();

	return 0;
}
