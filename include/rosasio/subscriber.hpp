#include <boost/asio.hpp>
#include <ros/message_traits.h>
#include <ros/serialization.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

namespace rosasio
{
    template <class MsgType>
    class PublisherConnection
    {
    public:
        PublisherConnection(boost::asio::ip::tcp::socket &&sock, const std::string &topic_name, const std::string &node_name, std::function<void(const MsgType &)> cb)
            : m_sock(std::move(sock)),
              m_topic_name(topic_name),
              m_node_name(node_name),
              m_cb(cb)
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
            } // namespace ros::serialization;

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&msglen, sizeof(msglen)),
                                    std::bind(&PublisherConnection::on_message_length_received, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void on_message_length_received(boost::system::error_code ec, std::size_t len)
        {
            buf.resize(msglen);
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(buf),
                                    std::bind(&PublisherConnection::on_message_received, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void on_message_received(boost::system::error_code ec, std::size_t len)
        {
            buf.resize(msglen);
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&msglen, sizeof(msglen)),
                                    std::bind(&PublisherConnection::on_message_length_received, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));

            namespace ser = ros::serialization;
            MsgType msg;
            ser::IStream stream(buf.data(), msglen);
            ser::deserialize(stream, msg);
            m_cb(msg);
        }

    private:
        boost::asio::ip::tcp::socket m_sock;
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
        Subscriber(const std::string &node_name, const std::string &topic_name, std::function<void(const MsgType &)> cb, boost::asio::io_context &ioc)
            : m_node_name(node_name),
              m_topic_name(topic_name),
              m_cb(cb),
              m_ioc(ioc)
        {
            auto publisher_uris = registerSubscriber(topic_name);

            for (auto &uri : publisher_uris)
            {
                auto tcpros_uri = request_topic(m_topic_name, uri);

                using boost::asio::ip::tcp;

                tcp::resolver resolver(m_ioc);
                tcp::resolver::query query(tcpros_uri.first, std::to_string(tcpros_uri.second));
                tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                tcp::resolver::iterator end;

                tcp::socket socket(m_ioc);
                boost::system::error_code error = boost::asio::error::host_not_found;
                while (error && endpoint_iterator != end)
                {
                    socket.close();
                    socket.connect(*endpoint_iterator++, error);

                    m_publisher_connections.emplace_back(
                        std::move(socket),
                        m_topic_name,
                        m_node_name,
                        m_cb);
                }
            }
        }

        ~Subscriber()
        {
            unregisterSubscriber(m_topic_name);
        }

        std::pair<std::string, int> request_topic(const std::string &topic_name, const std::string &uri)
        {
            using namespace std;

            const std::string methodName("requestTopic");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(uri, methodName, "ss((s))", &result, m_node_name.c_str(), topic_name.c_str(), "TCPROS");

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const vector<xmlrpc_c::value> uri_details(xmlrpc_c::value_array(param1Value[2]).vectorValueValue());

            std::cout << code << " : " << statusMessage << '\n';

            std::pair<std::string, int> ret;
            ret.first = xmlrpc_c::value_string(uri_details[1]);
            ret.second = xmlrpc_c::value_int(uri_details[2]);

            return ret;
        }

        std::vector<std::string> registerSubscriber(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("registerSubscriber");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "ssss", &result, m_node_name.c_str(), topic_name.c_str(), type, "rosrpc://zeus:12345");

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

            return ret;
        }

        void unregisterSubscriber(const std::string &topic_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("unregisterSubscriber");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            auto type = ros::message_traits::DataType<MsgType>::value();
            myClient.call(serverUrl, methodName, "sss", &result, m_node_name.c_str(), topic_name.c_str(), "rosrpc://zeus:12345");

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
        std::list<PublisherConnection<MsgType>> m_publisher_connections;
    };
} // namespace rosasio
