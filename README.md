# rosasio

## Introduction
- Everyone's getting excited about ROS2 but that doesn't mean we can still have some fun with the original ROS
- rosasio is a rewrite of the roscpp library designed around of `boost::asio`
- The main reason to use this is library over roscpp is so you can use a single event loop for ROS and any other event driven IO that boost::asio supports, such as:
  - Serial devices
  - Raw sockets
  - HTTP servers/clients
  - Any arbitrary file descriptor
- ... without having to resort to using threads or a polling approach.
- Primary goals:
  - Similar API to the current roscpp library, while also allowing users to get access to the `boost::asio::io_context` in order to use it for their own IO.
  - Single threaded, avoiding bugs caused by data races
  - More responsive than roscpp (no arbitrary sleeps)
  - More efficient and higher throughput of smaller messages compared to roscpp
- Secondary goals:
  - Optional async service clients
  - Only implement what you need (bring your own logger, timers, etc)
  - Allows multiple nodes in the same process & event loop

## Examples
See the /examples folder for example clients.
