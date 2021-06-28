#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <rosasio/node.hpp>

#include <ros/message_traits.h>
#include <ros/serialization.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

namespace rosasio
{
    template <class MsgType>
    struct PublisherConnectionBase : public std::enable_shared_from_this<PublisherConnectionBase<MsgType>>
    {
        virtual ~PublisherConnectionBase() = default;
    };

    template <class Protocol, class MsgType>
    class PublisherConnection : public PublisherConnectionBase<MsgType>
    {
    public:
        PublisherConnection(boost::asio::basic_stream_socket<Protocol> &&sock, const std::string &topic_name, const std::string &node_name, std::function<void(const MsgType &)> cb)
            : m_sock(std::move(sock)),
              m_topic_name(topic_name),
              m_node_name(node_name),
              m_cb(cb)
        {
            //
        }

        void start()
        {
            std::cout << "Connected to publisher\n";

            Message msg;
            msg.add_field("message_definition=string-data\n\n");
            msg.add_field("callerid", m_node_name);
            msg.add_field("topic", m_topic_name);
            msg.add_field("md5sum", ros::message_traits::MD5Sum<MsgType>::value());
            msg.add_field("type", ros::message_traits::DataType<MsgType>::value());
            msg.finish();
            boost::asio::write(m_sock, boost::asio::buffer(msg.buf));

            // TODO do this async
            {
                // TODO handle errors here
                boost::system::error_code error;

                uint32_t msglen;
                size_t len = boost::asio::read(m_sock, boost::asio::buffer(&msglen, sizeof(msglen)), error);
                // if (error == boost::asio::error::eof)
                //     break; // Connection closed cleanly by peer.
                // else if (error)
                //     throw boost::system::system_error(error); // Some other error.

                uint8_t buf[msglen];
                len = boost::asio::read(m_sock, boost::asio::buffer(buf, sizeof(buf)), error);

                // if (error == boost::asio::error::eof)
                //     break; // Connection closed cleanly by peer.
                // else if (error)
                //     throw boost::system::system_error(error); // Some other error.
            }

            auto shared_this = std::dynamic_pointer_cast<PublisherConnection<Protocol, MsgType>>(this->shared_from_this());

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&msglen, sizeof(msglen)),
                                    std::bind(&PublisherConnection::on_message_length_received, shared_this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        // virtual ~PublisherConnection() = default;

        // PublisherConnection(const PublisherConnection<MsgType> &) = delete;
        // PublisherConnection<MsgType> &operator=(const PublisherConnection<MsgType> &) = delete;

        // PublisherConnection(PublisherConnection<MsgType> &&) = default;
        // PublisherConnection<MsgType> &operator=(PublisherConnection<MsgType> &&) = default;

        void on_message_length_received(boost::system::error_code ec, std::size_t len)
        {
            if (ec)
            {
                std::cout << "Publisher connection terminated\n";
                return;
            }

            auto shared_this = std::dynamic_pointer_cast<PublisherConnection<Protocol, MsgType>>(this->shared_from_this());

            buf.resize(msglen);
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(buf),
                                    std::bind(&PublisherConnection::on_message_received, shared_this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void on_message_received(boost::system::error_code ec, std::size_t len)
        {
            if (ec)
            {
                std::cout << "Publisher connection terminated\n";
                return;
            }

            auto shared_this = std::dynamic_pointer_cast<PublisherConnection<Protocol, MsgType>>(this->shared_from_this());

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&msglen, sizeof(msglen)),
                                    std::bind(&PublisherConnection::on_message_length_received, shared_this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));

            namespace ser = ros::serialization;
            MsgType msg;
            ser::IStream stream(buf.data(), msglen);
            ser::deserialize(stream, msg);
            m_cb(msg);
        }

    private:
        boost::asio::basic_stream_socket<Protocol> m_sock;
        std::string m_topic_name;
        std::string m_node_name;
        std::function<void(const MsgType &)> m_cb;
        uint32_t msglen;
        std::vector<uint8_t> buf;
    };

    template <class MsgType>
    class Subscriber
    {
    public:
        Subscriber(rosasio::Node &node, const std::string &topic_name, std::function<void(const MsgType &)> cb)
            : m_node(node),
              m_topic_name(topic_name),
              m_cb(cb),
              m_ioc(node.get_ioc())
        {
            auto publisher_xmlrpc_uris = node.register_subscriber<MsgType>(topic_name, std::bind(&Subscriber::notify_new_pubs, this, std::placeholders::_1));

            for (auto &publisher_xmlrpc_uri : publisher_xmlrpc_uris)
            {
                try
                {
                    create_publisher_connection(publisher_xmlrpc_uri);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error connecting to publisher's XMLRPC server: " << e.what() << '\n';
                }
            }
        }

        virtual ~Subscriber()
        {
            m_node.unregister_subscriber<MsgType>(m_topic_name);
        }

        void notify_new_pubs(std::vector<std::string> publisher_xmlrpc_uris)
        {
            std::cout << "Got a publisher update!\n";

            for (auto publisher_xmlrpc_uri : publisher_xmlrpc_uris)
            {
                auto iter = m_publisher_connections.find(publisher_xmlrpc_uri);
                if (iter == m_publisher_connections.end())
                {
                    create_publisher_connection(publisher_xmlrpc_uri);
                }
            }
        }

        std::pair<std::string, int> request_topic(const std::string &topic_name, const std::string &uri)
        {
            using namespace std;

            const std::string methodName("requestTopic");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(uri, methodName, "ss((s))", &result, m_node.get_name().c_str(), topic_name.c_str(), "TCPROS");

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const vector<xmlrpc_c::value> uri_details(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());
            const vector<xmlrpc_c::value> uds_details(xmlrpc_c::value_array(param1Value[3]).vectorValueValue());

            std::cout << code << " : " << statusMessage << '\n';

            std::pair<std::string, int> ret;
            ret.first = xmlrpc_c::value_string(uds_details[1]);
            ret.second = xmlrpc_c::value_int(uri_details[2]);

            std::string uds_socket = xmlrpc_c::value_string(uds_details[1]);
            std::cout << "UDS socket:" << uds_socket << "\n";

            return ret;
        }

        void create_publisher_connection(const std::string &publisher_xmlrpc_uri)
        {
            try
            {
                auto tcpros_uri = request_topic(m_topic_name, publisher_xmlrpc_uri);

                boost::asio::local::stream_protocol::endpoint ep(tcpros_uri.first);
                boost::asio::local::stream_protocol::socket socket(m_ioc);
                socket.connect(ep);

                auto conn = std::make_shared<PublisherConnection<boost::asio::local::stream_protocol, MsgType>>(
                    std::move(socket),
                    m_topic_name,
                    m_node.get_name(),
                    m_cb);

                conn->start();

                m_publisher_connections.emplace(
                    publisher_xmlrpc_uri,
                    conn);

                // using boost::asio::ip::tcp;

                // tcp::resolver resolver(m_ioc);
                // tcp::resolver::query query(tcpros_uri.first, std::to_string(tcpros_uri.second));
                // tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                // tcp::resolver::iterator end;

                // tcp::socket socket(m_ioc);
                // boost::system::error_code error = boost::asio::error::host_not_found;
                // while (error && endpoint_iterator != end)
                // {
                //     socket.close();
                //     socket.connect(*endpoint_iterator++, error);

                //     auto conn = std::make_shared<PublisherConnection<boost::asio::ip::tcp, MsgType>>(
                //         std::move(socket),
                //         m_topic_name,
                //         m_node.get_name(),
                //         m_cb);

                //     conn->start();

                //     m_publisher_connections.emplace(
                //         publisher_xmlrpc_uri,
                //         conn);
                // }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error connecting to publisher's XMLRPC server: " << e.what() << '\n';
            }
        }

        rosasio::Node &m_node;
        std::string m_topic_name;
        std::function<void(const MsgType &)> m_cb;
        boost::asio::io_context &m_ioc;
        std::map<std::string, std::shared_ptr<PublisherConnectionBase<MsgType>>> m_publisher_connections;
    };
} // namespace rosasio
