#include <string>
#include <boost/asio.hpp>

// TODO remove me eventually
#include <iostream>

#include "header.hpp"
#include "timer.hpp"
#include "service_client.hpp"
#include "subscriber.hpp"

namespace rosasio
{
    class Node
    {
    public:
        Node(const std::string &name)
            : m_name(name)
        {
            // TODO register node
        }

        template <class MsgType>
        std::shared_ptr<ServiceClient<MsgType>> service_client(const std::string &service_name)
        {
            return std::make_shared<ServiceClient<MsgType>>(m_name, service_name);
        }

        template <class MsgType>
        std::shared_ptr<Subscriber<MsgType>> subscribe(const std::string &topic, std::function<void(const MsgType &)> cb)
        {
            return std::make_shared<Subscriber<MsgType>>(m_name, topic, cb, ioc);
        }

        std::shared_ptr<Timer> create_timer(const std::chrono::milliseconds &interval, std::function<void()> cb)
        {
            return std::make_shared<Timer>(interval, cb, ioc);
        }

        void spin()
        {
            ioc.run();
        }

        boost::asio::io_context ioc;
        std::string m_name;
    };

} // namespace rosasio
