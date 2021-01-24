#pragma once

#include <string>
#include <boost/asio.hpp>

// TODO remove me eventually
#include <iostream>

#include "header.hpp"
#include "timer.hpp"
#include "service_client.hpp"
#include "subscriber.hpp"
#include "publisher.hpp"

#include "xmlrpc_server.hpp"

namespace rosasio
{
    class Node
    {
    public:
        Node(const std::string &name)
            : m_name(name),
              signals(ioc, SIGINT, SIGTERM),
              m_xmlrpc_server(ioc)
        {
            // TODO register node??
            signals.async_wait([this](const boost::system::error_code &ec,
                                      int signal_number) {
                if (!ec)
                    ioc.stop();
            });

            m_xmlrpc_server.register_method("requestTopic", [](auto &params) {
                std::cout << "requestTopic\n";
                // Make the vector value 'arrayData'
                std::vector<xmlrpc_c::value> arrayData;
                arrayData.push_back(xmlrpc_c::value_int(1));
                arrayData.push_back(xmlrpc_c::value_string("OK"));

                std::vector<xmlrpc_c::value> ep;
                ep.push_back(xmlrpc_c::value_string("TCPROS"));
                ep.push_back(xmlrpc_c::value_string("localhost"));
                ep.push_back(xmlrpc_c::value_int(12346));
                xmlrpc_c::value_array ep_xml(ep);

                arrayData.push_back(ep_xml);

                // Make an XML-RPC array out of it
                xmlrpc_c::value_array array1(arrayData);
                xmlrpc_c::rpcOutcome outcome(array1);
                return outcome;
            });
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

        template <class MsgType>
        std::shared_ptr<Publisher<MsgType>> advertise(const std::string &topic)
        {
            return std::make_shared<Publisher<MsgType>>(m_name, topic, ioc);
        }

        std::shared_ptr<Timer> create_timer(const std::chrono::milliseconds &interval, std::function<void()> cb)
        {
            return std::make_shared<Timer>(interval, cb, ioc);
        }

        void spin()
        {
            // TODO maybe wait on signals here?
            ioc.run();
        }

        boost::asio::io_context ioc;
        std::string m_name;
        boost::asio::signal_set signals;
        rosasio::XmlRpcServer m_xmlrpc_server;
    };

} // namespace rosasio
