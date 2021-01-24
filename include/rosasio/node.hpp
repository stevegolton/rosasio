#include <string>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/regex.hpp>
#include <cstdint>
#include <vector>
#include <functional>
#include <ros/service_traits.h>
#include <ros/serialization.h>

// TODO remove me
#include <iostream>

namespace rosasio
{
    class Message
    {
    public:
        Message()
            : buf(sizeof(uint32_t), 0)
        {
            //
        }

        void add_field(const std::string &name, const std::string &value)
        {
            add_field(name + "=" + value);
        }

        void add_field(const std::string &value)
        {
            uint32_t len = value.size();
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&len);
            std::copy(bytes, bytes + sizeof(len), std::back_inserter(buf));
            std::copy(value.begin(), value.end(), std::back_inserter(buf));
        }

        void finish()
        {
            uint32_t len = buf.size() - sizeof(uint32_t);
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&len);
            memcpy(buf.data(), bytes, sizeof(len));
        }

        std::vector<unsigned char> buf;
    };

    template <class MsgType>
    class ServiceClient
    {
    public:
        ServiceClient(const std::string &node_name, const std::string &service_name)
            : m_node_name(node_name),
              m_service_name(service_name)
        {
            //
        }

        void call(MsgType &srv)
        {
            auto uri = lookupService(m_service_name);
            call_service(uri, srv);
        }

    private:
        void call_service(const std::string &uri, MsgType &srv)
        {
            using boost::asio::ip::tcp;

            boost::asio::io_service io_service;

            boost::regex exrp("^rosrpc://(.*):(.*)$");
            boost::match_results<std::string::const_iterator> what;
            if (regex_search(uri, what, exrp))
            {
                std::string host(what[1].first, what[1].second);
                std::string port(what[2].first, what[2].second);

                std::cout << port << '\n';

                tcp::resolver resolver(io_service);
                tcp::resolver::query query(host, port);
                tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                tcp::resolver::iterator end;

                tcp::socket socket(io_service);
                boost::system::error_code error = boost::asio::error::host_not_found;
                while (error && endpoint_iterator != end)
                {
                    socket.close();
                    socket.connect(*endpoint_iterator++, error);
                }
                if (error)
                    throw boost::system::system_error(error);

                {
                    Message msg;
                    msg.add_field("message_definition=string-data\n\n");
                    msg.add_field("callerid", m_node_name);
                    msg.add_field("service", m_service_name);
                    msg.add_field("md5sum", ros::service_traits::MD5Sum<MsgType>::value());
                    msg.finish();
                    boost::asio::write(socket, boost::asio::buffer(msg.buf));
                }

                namespace ser = ros::serialization;

                {
                    boost::system::error_code error;

                    uint32_t msglen;
                    size_t len = boost::asio::read(socket, boost::asio::buffer(&msglen, sizeof(msglen)), error);

                    std::cout << "Reading: " << msglen << " bytes\n";

                    // if (error == boost::asio::error::eof)
                    //     break; // Connection closed cleanly by peer.
                    // else if (error)
                    //     throw boost::system::system_error(error); // Some other error.

                    uint8_t buf[msglen];
                    len = boost::asio::read(socket, boost::asio::buffer(buf, sizeof(buf)), error);

                    std::cout << "Read: " << msglen << " bytes\n";

                    // if (error == boost::asio::error::eof)
                    //     break; // Connection closed cleanly by peer.
                    // else if (error)
                    //     throw boost::system::system_error(error); // Some other error.
                } // namespace ros::serialization;

                {
                    uint32_t serial_size = ros::serialization::serializationLength(srv.request);
                    boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

                    ser::OStream stream(buffer.get(), serial_size);
                    ser::serialize(stream, srv.request);

                    boost::asio::write(socket, boost::asio::buffer(&serial_size, sizeof(serial_size)));
                    boost::asio::write(socket, boost::asio::buffer(buffer.get(), serial_size));
                }

                {
                    boost::system::error_code error;

                    uint8_t stat;
                    size_t len = boost::asio::read(socket, boost::asio::buffer(&stat, sizeof(stat)), error);
                    std::cout << "Stat: " << stat << "\n";

                    uint32_t msglen;
                    len = boost::asio::read(socket, boost::asio::buffer(&msglen, sizeof(msglen)), error);
                    std::cout << "Reading: " << msglen << " bytes\n";

                    uint8_t buf[msglen];
                    len = boost::asio::read(socket, boost::asio::buffer(buf, sizeof(buf)), error);

                    // if (error == boost::asio::error::eof)
                    //     break; // Connection closed cleanly by peer.
                    // else if (error)
                    //     throw boost::system::system_error(error); // Some other error.

                    std::cout << "Read: " << msglen << " bytes\n";

                    // if (error == boost::asio::error::eof)
                    //     break; // Connection closed cleanly by peer.
                    // else if (error)
                    //     throw boost::system::system_error(error); // Some other error.

                    // Fill buffer with a serialized UInt32
                    ser::IStream stream(buf, msglen);
                    ser::deserialize(stream, srv.response);
                }
            }
        }

        std::string lookupService(const std::string &service_name)
        {
            using namespace std;

            const std::string serverUrl("http://localhost:11311");
            const std::string methodName("lookupService");

            xmlrpc_c::clientSimple myClient;
            xmlrpc_c::value result;

            myClient.call(serverUrl, methodName, "ss", &result, m_node_name.c_str(), service_name.c_str());

            xmlrpc_c::value_array arr(result);
            const vector<xmlrpc_c::value> param1Value(arr.vectorValueValue());

            const int code = xmlrpc_c::value_int(param1Value[0]);
            const std::string statusMessage = xmlrpc_c::value_string(param1Value[1]);
            const std::string serviceUrl = xmlrpc_c::value_string(param1Value[2]);

            return serviceUrl;
        }

        std::string m_node_name;
        std::string m_service_name;
    };

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

    class Timer
    {
    public:
        Timer(const std::chrono::milliseconds &interval, std::function<void()> cb, boost::asio::io_context &ioc)
            : m_interval(interval),
              m_timer(ioc, interval),
              m_cb(cb)
        {
            m_timer.async_wait(std::bind(&Timer::timer_handler, this, std::placeholders::_1));
        }

    private:
        void timer_handler(boost::system::error_code ec)
        {
            if (ec)
                return;

            m_timer.expires_at(m_timer.expiry() + m_interval);
            m_timer.async_wait(std::bind(&Timer::timer_handler, this, std::placeholders::_1));

            m_cb();
        }

        std::chrono::milliseconds m_interval;
        boost::asio::steady_timer m_timer;
        std::function<void()> m_cb;
    };

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
