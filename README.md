# rosasio

## Introduction
- rosasio is a complete rewrite of the `ros_comm` library designed around `boost::asio`
- The main reason to use this is library over `ros_comm` is in order to use a single event loop for ROS and any other event driven IO that `boost::asio` supports, such as:
  - Serial devices
  - Raw sockets
  - HTTP + websockets via `boost::beast`
  - A variety of timers via`boost::asio::*_timer`
  - filesystem, socketcan, joydev, eventfd, pipes, mqueues, or any other file descriptor via `boost::asio::posix::stream_descriptor`
- The main use case for this library is for drivers which usually communicate with some external device. Traditionally this is done using another thread or a polled approach, but this library allows the same driver to be written using a single threaded approach.
- Primary goals:
  - Similar API to the current roscpp library, while also exposing the underlying `boost::asio::io_context`.
  - Entirely single threaded, which improves stability and removes an entire category of bugs.
  - Setting up and tearing down subscriptions should be more responsive than `ros_comm` (no arbitrary sleeps).
  - More efficient and higher throughput of smaller messages compared to `ros_comm`.
- Secondary goals:
  - Optional async service clients.
  - Only implement what you need (bring your own logger, timers, etc).
  - Allow multiple nodes in the same process using the same event loop.
  - Unix domain sockets as an alternative to TCP sockets.
- Missing features:
  - Efficient interprocess communication.
  - ROS logging macros + logging to `/rosout`.

## Examples
See the /src folder for example clients.
