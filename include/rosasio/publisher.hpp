#pragma once

#include <boost/asio.hpp>
#include <ros/message_traits.h>
#include <ros/serialization.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

namespace rosasio
{
    template <class MsgType>
    class SubscriberConnection : public std::enable_shared_from_this<SubscriberConnection<MsgType>>
    {
    public:
        SubscriberConnection(boost::asio::ip::tcp::socket &&sock, std::string node_name, std::string topic_name)
            : m_sock(std::move(sock)),
              initialized(false),
              m_node_name(node_name),
              m_topic_name(topic_name)
        {
        }

        void publish(const MsgType &msg)
        {
            if (!initialized)
                return;

            namespace ser = ros::serialization;

            uint32_t serial_size = ros::serialization::serializationLength(msg);
            boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

            ser::OStream stream(buffer.get(), serial_size);
            ser::serialize(stream, msg);

            boost::asio::write(m_sock, boost::asio::buffer(&serial_size, sizeof(serial_size)));
            boost::asio::write(m_sock, boost::asio::buffer(buffer.get(), serial_size));
        }

        void start()
        {
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&SubscriberConnection::handle_read, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read(boost::system::error_code ec, std::size_t bytes_read)
        {
            std::string s(m_buffer.begin(), m_buffer.end());
            std::cout << "Read happened: " << s << "\n";

            Message msg;
            msg.add_field("message_definition=string-data\n\n");
            msg.add_field("callerid", m_node_name);
            // msg.add_field("topic", m_topic_name);
            msg.add_field("md5sum", ros::message_traits::MD5Sum<MsgType>::value());
            msg.add_field("type", ros::message_traits::DataType<MsgType>::value());
            msg.finish();
            boost::asio::write(m_sock, boost::asio::buffer(msg.buf));
            initialized = true;
        }

        boost::asio::ip::tcp::socket m_sock;

    private:
        uint32_t m_msglen;
        std::vector<uint8_t> m_buffer;
        bool initialized;
        std::string m_node_name;
        std::string m_topic_name;
    };

    template <class MsgType>
    class Publisher
    {
    public:
        Publisher(const std::string &node_name, const std::string &topic_name, boost::asio::io_context &ioc)
            : m_node_name(node_name),
              m_topic_name(topic_name),
              m_ioc(ioc),
              m_acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12346))
        {
            registerPublisher(topic_name);
            start_accept();
        }

        ~Publisher()
        {
            unregisterPublisher(m_topic_name);
        }

        void publish(const MsgType &msg)
        {
            for (auto conn : m_subscriber_connections)
            {
                conn->publish(msg);
            }
        }

    private:
        void start_accept()
        {
            boost::asio::ip::tcp::socket sock(m_ioc);

            // TODO we don't really need to move here - just create the socket as part of the connection object amd pass in ref to ioc
            auto conn = std::make_shared<SubscriberConnection<MsgType>>(std::move(sock), m_node_name, m_topic_name);

            m_acceptor.async_accept(conn->m_sock,
                                    std::bind(
                                        &Publisher::accept_handler, this,
                                        conn,
                                        std::placeholders::_1));
        }

        void accept_handler(std::shared_ptr<SubscriberConnection<MsgType>> conn, const boost::system::error_code &error)
        {
            if (!error)
            {
                std::cout << "Accepted connection\n";
                conn->start();
                m_subscriber_connections.push_back(conn);
            }

            start_accept();
        }

        boost::asio::ip::tcp::acceptor m_acceptor;

        // std::pair<std::string, int> request_topic(const std::string &topic_name, const std::string &uri)
        // {
        //     using namespace std;

        //     const std::string methodName("requestTopic");

        //     xmlrpc_c::clientSimple myClient;
        //     xmlrpc_c::value result;

        //     auto type = ros::message_traits::DataType<MsgType>::value();
        //     myClient.call(uri, methodName, "ss((s))", &result, m_node_name.c_str(), topic_name.c_str(), "TCPROS");

        //     xmlrpc_c::value_array arr(result);
        //     const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

        //     const int code = xmlrpc_c::value_int(param1Value[0]);
        //     const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
        //     const vector<xmlrpc_c::value> uri_details(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());

        //     std::cout << code << " : " << statusMessage << '\n';

        //     std::pair<std::string, int> ret;
        //     ret.first = xmlrpc_c::value_string(uri_details[1]);
        //     ret.second = xmlrpc_c::value_int(uri_details[2]);

        //     return ret;
        // }

        std::vector<std::string> registerPublisher(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("registerPublisher");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "ssss", &result, m_node_name.c_str(), topic_name.c_str(), type, "http://localhost:12345");

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const vector<xmlrpc_c::value> publishers(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());

            std::cout << code << ", " << statusMessage << '\n';

            std::vector<std::string> ret;
            // for (auto &pub : publishers)
            // {
            //     const std::string pubby = xmlrpc_c::value_string(pub);
            //     std::cout << "publisher uri: " << pubby << "\n";

            //     ret.push_back(pubby);
            // }

            return ret;
        }

        void unregisterPublisher(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("unregisterPublisher");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "sss", &result, m_node_name.c_str(), topic_name.c_str(), "rosrpc://localhost:12345");

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);

            std::cout << code << ", " << statusMessage << '\n';
        }

        std::string m_node_name;
        std::string m_topic_name;
        std::function<void(const MsgType &)> m_cb;
        boost::asio::io_context &m_ioc;
        std::list<std::shared_ptr<SubscriberConnection<MsgType>>> m_subscriber_connections;
    };
} // namespace rosasio
