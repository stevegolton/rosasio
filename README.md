# rosasio

TODO: un bullet-point this readme!

## Introduction
- rosasio is an aggressively single-threaded ROS client library for C++ applications
- Goals:
  - smaller, less buggy and more responsive than roscpp
  - Cross platform support without #ifdef WIN32
  - No arbitrary sleeps
  - Single threaded; avoid bugs caused by data races
- Takes advantage of modern networking libraries like Boost ASIO, Boost Beast, and xmlrpc-c, which have appeared since roscpp was first written over a decade ago!
- Similar API to the current roscpp library, while also allowing users to get access to the `io_context` in order to use it for their own blocking IO.
  - All IO can be handled using one event loop, and only one thread is required!
  - Begone locks, begone polling, begone data races, begone arbitrary `ros::Duration(...).sleep()`s!
- A long term goal to support thread pool (call ioc.run() from multiple threads) for improved performance on multi-core systems. Access to shared memory inside the client library should be protected using strands rather than locks.

- I realise this project is already obsolete due to the transition to ROS2, and a lot of the goals of this project are already covered by the ROS2 client libraries, but I admire the simplicity of the ROS1 protocol and hopefully I can learn something along the way!
- Everyone's getting excited about ROS2 but that doesn't mean we can still have some fun with the original ROS!

## Background
- Increasingly frustrated with the ROS C++ client library
- Encountering a few bugs recently that are a result of data races, or performance bottlenecks which are are result of arbitrary sleeps while waiting for other threads to do stuff.
- I dislike the way threads are used in most areas of programming
- Especially when they are used to parallelize IO operations
- I'm a strong believer that threading should never be required for the core functionality of your application
  - should be used to improve performance only.
- Unless used carefully, threads can very quickly start to cause bugs due to data races, the infamous heisenbugs, and can just generally ruin your day you're trying to debug an applications.
- What's more, constantly switching contexts, waiting on locks and condition variables is not particularly efficient either.
- You might be tempted to use threading to speed up performance on multi core systems, but unless they are used carefully, you'll probably end up with a program which is more complex, less stable, significantly harder to debug, and not actually any more performant.

- I've recently been working on a project which involves writing javascript designed to run by a browser, aka "web development".
- Everyone likes to hate javascript but there is one outstanding feature: the event loop.
- Javascript is strictly single threaded, and all blocking IO is done using the this ubiquitous event loop.
- Because of it ubiquity, all libraries use the event loop for any blocking IO.
- The amazing this is that javascript can be amazingly responsive and performant, despite the lack of threading.

- To be clear, there are a few special cases where threads _are_ a good idea:
  - Real-time applications where preemption is necessary to meet deadlines
    - you're probably better off using processes with isolated address spaces, using queues provided by the OS for IPC, assuming your platform supports an OS that can do this - tiny micros with no MMU are likely the exception.
  - Performance optimizations for parallelizing CPU-bound algorithms
    - don't arbitrarily spin up your own threads, use a thread pool!

- The current roscpp implementation uses threads, and thus there are bugs due by race conditions and performance issues caused by waiting on locks and condition variables, as wel as some arbitrary sleeps sprinkled into the mix.
- Because of this, it takes around 100ms to subscribe to a topic, and around 250ms before an action client can start to be used, for no other reason than because this is the arbitrary timeout the developers decided to use in their polling implementation.
- Service calls can just sometimes fail, for no other reason than the threads are not managed correctly.
- The annoying this is, there is no need for threads.
  - the library simply waits for one of a number of event to happen, does some processing, then goes back to waiting.
  - This can be done using a single thread and an event loop!
- What's more, the current roscpp forces the user to use threads (or polling), if they want to wait on any IO outside of ROS:
  - waiting on a raw socket
  - waiting on a serial port
  - waiting on a GPIO
  - making an HTTP call and waiting for a response
