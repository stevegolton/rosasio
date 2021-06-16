#pragma once

#include <string>
#include <boost/asio.hpp>
#include <ros/service_traits.h>

// TODO remove me eventually
#include <iostream>

#include "header.hpp"
#include "xmlrpc_server.hpp"

namespace rosasio
{
    typedef std::function<void(std::vector<std::string>)> SubscriberCallback;

    class SubscriberHandle
    {
    public:
        virtual void notify() = 0;
    };

    class Exec
    {
    public:
        Exec()
            : m_signals(m_ioc, SIGINT, SIGTERM)
        {
            m_signals.async_wait([this](const boost::system::error_code &ec,
                                        int signal_number) {
                (void)signal_number;
                if (!ec)
                    m_ioc.stop();
            });
        }

        boost::asio::io_context &get_ioc()
        {
            return m_ioc;
        }

        void run()
        {
            m_ioc.run();
        }

    private:
        boost::asio::io_context m_ioc;
        boost::asio::signal_set m_signals;
    };

    class Node
    {
    public:
        Node(boost::asio::io_context &ioc, const std::string &name)
            : m_ioc(ioc),
              m_name(name),
              m_xmlrpc_server(m_ioc),
              m_hostname(boost::asio::ip::host_name())
        {
            std::cout << "Started XMLRPC server at: http://" << m_hostname << ":" << m_xmlrpc_server.get_port() << "\n";

            m_xmlrpc_server.register_method("requestTopic", [this](auto &params) {

                try
                {
                    auto topic_name = params.getString(1);
                    auto iter = m_topics.find(topic_name);
                    if (iter != m_topics.end())
                    {
                        std::vector<xmlrpc_c::value> ep;
                        ep.push_back(xmlrpc_c::value_string("TCPROS"));
                        ep.push_back(xmlrpc_c::value_string(m_hostname));
                        ep.push_back(xmlrpc_c::value_int(iter->second));
                        xmlrpc_c::value_array ep_xml(ep);

                        std::vector<xmlrpc_c::value> arrayData;
                        arrayData.push_back(xmlrpc_c::value_int(1));
                        arrayData.push_back(xmlrpc_c::value_string("OK"));
                        arrayData.push_back(ep_xml);
                        xmlrpc_c::value_array array1(arrayData);

                        xmlrpc_c::rpcOutcome outcome(array1);

                        return outcome;
                    }
                    else
                    {
                        std::vector<xmlrpc_c::value> arrayData;
                        arrayData.push_back(xmlrpc_c::value_int(1));
                        arrayData.push_back(xmlrpc_c::value_string("OK"));
                        xmlrpc_c::value_array array1(arrayData);

                        xmlrpc_c::rpcOutcome outcome(array1);

                        return outcome;
                    }
                }
                catch (const xmlrpc_c::fault &e)
                {
                    std::vector<xmlrpc_c::value> arrayData;
                    arrayData.push_back(xmlrpc_c::value_int(1));
                    arrayData.push_back(xmlrpc_c::value_string("OOPS!"));
                    xmlrpc_c::value_array array1(arrayData);

                    xmlrpc_c::rpcOutcome outcome(array1);

                    return outcome;
                }
            });

            m_xmlrpc_server.register_method("lookupService", [this](auto &params) {
                auto service_name = params.getString(1);
                auto iter = m_services.find(service_name);
                if (iter != m_services.end())
                {
                    std::vector<xmlrpc_c::value> ep;
                    ep.push_back(xmlrpc_c::value_string("TCPROS"));
                    ep.push_back(xmlrpc_c::value_string(m_hostname));
                    ep.push_back(xmlrpc_c::value_int(iter->second));
                    xmlrpc_c::value_array ep_xml(ep);

                    std::vector<xmlrpc_c::value> arrayData;
                    arrayData.push_back(xmlrpc_c::value_int(1));
                    arrayData.push_back(xmlrpc_c::value_string("OK"));
                    arrayData.push_back(ep_xml);
                    xmlrpc_c::value_array array1(arrayData);

                    xmlrpc_c::rpcOutcome outcome(array1);

                    return outcome;
                }
                else
                {
                    std::vector<xmlrpc_c::value> arrayData;
                    arrayData.push_back(xmlrpc_c::value_int(1));
                    arrayData.push_back(xmlrpc_c::value_string("OK"));
                    xmlrpc_c::value_array array1(arrayData);

                    xmlrpc_c::rpcOutcome outcome(array1);

                    return outcome;
                }
            });

            m_xmlrpc_server.register_method("publisherUpdate", [this](auto &params) {
                auto topic_name = params.getString(1);
                std::vector<xmlrpc_c::value> publisher_uris = params.getArray(2);

                auto iter = m_subscribers.find(topic_name);
                if (iter != m_subscribers.end())
                {
                    std::vector<std::string> uris;

                    std::transform(
                        publisher_uris.begin(),
                        publisher_uris.end(),
                        std::back_inserter(uris),
                        [](const auto &value) {
                            return xmlrpc_c::value_string(value);
                        });

                    iter->second(uris);
                }

                std::vector<xmlrpc_c::value> arrayData;
                arrayData.push_back(xmlrpc_c::value_int(1));
                arrayData.push_back(xmlrpc_c::value_string("OK"));
                xmlrpc_c::value_array array1(arrayData);
                return xmlrpc_c::rpcOutcome(array1);
            });

            m_xmlrpc_server.register_method("getPid", [this](auto &) {

                std::vector<xmlrpc_c::value> arrayData;
                arrayData.push_back(xmlrpc_c::value_int(1));
                arrayData.push_back(xmlrpc_c::value_string("OK"));
                arrayData.push_back(xmlrpc_c::value_int(getpid()));
                xmlrpc_c::value_array array1(arrayData);

                xmlrpc_c::rpcOutcome outcome(array1);

                return outcome;
            });

            m_xmlrpc_server.register_method("shutdown", [this](auto &) {

                std::vector<xmlrpc_c::value> arrayData;
                arrayData.push_back(xmlrpc_c::value_int(1));
                arrayData.push_back(xmlrpc_c::value_string("OK"));
                xmlrpc_c::value_array array1(arrayData);
                xmlrpc_c::rpcOutcome outcome(array1);

                m_ioc.stop();

                return outcome;
            });
        }

        boost::asio::io_context &get_ioc() const
        {
            return m_ioc;
        }

        std::string get_name() const
        {
            return m_name;
        }

        // template <class MsgType>
        // std::shared_ptr<ServiceClient<MsgType>> service_client(const std::string &service_name)
        // {
        //     return std::make_shared<ServiceClient<MsgType>>(m_name, service_name);
        // }

        // template <class MsgType>
        // std::shared_ptr<Subscriber<MsgType>> subscribe(const std::string &topic, std::function<void(const MsgType &)> cb)
        // {
        //     return std::make_shared<Subscriber<MsgType>>(m_name, topic, cb, m_ioc);
        // }

        // template <class MsgType>
        // std::shared_ptr<Publisher<MsgType>> advertise(const std::string &topic)
        // {
        //     return std::make_shared<Publisher<MsgType>>(m_name, topic, m_ioc);
        // }

        /**
         * @brief Should be called from publishers to register with the node that they have a topic they would like to advertise.
         * 
         * @param topic_name 
         */
        template <class MsgType>
        void register_publisher(const std::string &topic_name, unsigned short port)
        {
            using namespace std;

            cout << "Registering publisher on topic " << topic_name << " using port " << port << "\n";

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("registerPublisher");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "http://" << m_hostname << ":" << m_xmlrpc_server.get_port();

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "ssss", &result, m_name.c_str(), topic_name.c_str(), type, uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const vector<xmlrpc_c::value> publishers(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());

            std::cout << code << ", " << statusMessage << '\n';

            std::vector<std::string> ret;

            m_topics.insert(std::make_pair(topic_name, port));
        }

        template <class MsgType>
        void unregister_publisher(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("unregisterPublisher");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "http://" << m_hostname << ":" << m_xmlrpc_server.get_port();

            myClient.call(serverUrl, methodName, "sss", &result, m_name.c_str(), topic_name.c_str(), uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);

            std::cout << code << ", " << statusMessage << '\n';

            m_topics.erase(topic_name);
        }

        template <class MsgType>
        std::vector<std::string> register_subscriber(const std::string &topic_name, SubscriberCallback cb)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("registerSubscriber");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "http://" << m_hostname << ":" << m_xmlrpc_server.get_port();

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "ssss", &result, m_name.c_str(), topic_name.c_str(), type, uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const vector<xmlrpc_c::value> publishers(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());

            std::cout << code << ", " << statusMessage << '\n';

            std::vector<std::string> ret;
            for (auto &pub : publishers)
            {
                const std::string pubby = xmlrpc_c::value_string(pub);
                std::cout << "publisher uri: " << pubby << "\n";

                ret.push_back(pubby);
            }

            m_subscribers.insert(std::make_pair(topic_name, cb));

            return ret;
        }

        template <class MsgType>
        void unregister_subscriber(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("unregisterSubscriber");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "http://" << m_hostname << ":" << m_xmlrpc_server.get_port();

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "sss", &result, m_name.c_str(), topic_name.c_str(), uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);

            std::cout << code << ", " << statusMessage << '\n';

            m_subscribers.erase(topic_name);
        }

        template <class MsgType>
        void register_service(const std::string &service_name, unsigned short port)
        {
            using namespace std;

            cout << "Registering service on " << service_name << " using port " << port << "\n";

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("registerService");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "rosrpc://" << m_hostname << ":" << port;

            std::stringstream http_uri;
            http_uri << "http://" << m_hostname << ":" << m_xmlrpc_server.get_port();

            // auto type = ros::service_traits::DataType<MsgType>::value();
            myClient.call(
                serverUrl, methodName, "ssss", &result,
                m_name.c_str(),
                service_name.c_str(),
                uri.str().c_str(),
                http_uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);

            std::cout << code << ", " << statusMessage << '\n';

            std::vector<std::string> ret;

            m_services.insert(std::make_pair(service_name, port));
        }

        template <class MsgType>
        void unregister_service(const std::string &service_name, unsigned short port)
        {
            using namespace std;

            cout << "Unregistering service on " << service_name << " using port " << port << "\n";

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("unregisterService");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            std::stringstream uri;
            uri << "rosrpc://" << m_hostname << ":" << port;

            // auto type = ros::service_traits::DataType<MsgType>::value();
            myClient.call(
                serverUrl, methodName, "sss", &result,
                m_name.c_str(),
                service_name.c_str(),
                uri.str().c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);

            std::cout << code << ", " << statusMessage << '\n';

            std::vector<std::string> ret;

            m_services.erase(service_name);
        }

        boost::asio::io_context &m_ioc;
        std::string m_name;
        rosasio::XmlRpcServer m_xmlrpc_server;
        std::string m_hostname;
        std::map<std::string, unsigned short> m_topics;
        std::map<std::string, SubscriberCallback> m_subscribers;
        std::map<std::string, unsigned short> m_services;
    };

} // namespace rosasio
