#pragma once

#include <boost/asio.hpp>
#include <rosasio/node.hpp>

#include <ros/message_traits.h>
#include <ros/serialization.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

namespace rosasio
{
    template <class SubscriberType>
    class SubscriberPool
    {
    public:
        std::set<std::shared_ptr<SubscriberType>> subscriber_connections;
    };

    template <class MsgType>
    class SubscriberConnection : public std::enable_shared_from_this<SubscriberConnection<MsgType>>
    {
    public:
        SubscriberConnection(boost::asio::ip::tcp::socket &&sock, std::string node_name, SubscriberPool<SubscriberConnection<MsgType>> &pool)
            : m_sock(std::move(sock)),
              m_node_name(node_name),
              m_pool(pool)
        {
        }

        void start()
        {
            using namespace std::placeholders;
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&SubscriberConnection::handle_read_len, this->shared_from_this(), _1, _2));
        }

        void publish(const MsgType &msg)
        {
            namespace ser = ros::serialization;

            uint32_t serial_size = ros::serialization::serializationLength(msg);
            boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

            ser::OStream stream(buffer.get(), serial_size);
            ser::serialize(stream, msg);

            try
            {

                boost::asio::write(m_sock, boost::asio::buffer(&serial_size, sizeof(serial_size)));
                boost::asio::write(m_sock, boost::asio::buffer(buffer.get(), serial_size));
            }
            catch (const boost::system::system_error &e)
            {
            }
        }

        boost::asio::ip::tcp::socket m_sock;

    private:
        void handle_read_len(boost::system::error_code ec, std::size_t)
        {
            using namespace std::placeholders;

            if (ec)
                return;

            m_buffer.reserve(m_msglen);

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(m_buffer),
                                    std::bind(&SubscriberConnection::handle_read, this->shared_from_this(), _1, _2));
        }

        void handle_read(boost::system::error_code ec, std::size_t)
        {
            if (ec)
                return;

            using namespace std::placeholders;

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&SubscriberConnection::handle_close, this->shared_from_this(), _1, _2));

            Message msg;
            msg.add_field("message_definition=string-data\n\n");
            msg.add_field("callerid", m_node_name);
            msg.add_field("md5sum", ros::message_traits::MD5Sum<MsgType>::value());
            msg.add_field("type", ros::message_traits::DataType<MsgType>::value());
            msg.finish();

            boost::asio::write(m_sock, boost::asio::buffer(msg.buf));
            std::cout << "Adding subscriber connection\n";
            m_pool.subscriber_connections.insert(this->shared_from_this());
        }

        void handle_close(boost::system::error_code ec, std::size_t)
        {
            using namespace std::placeholders;

            if (ec == boost::asio::error::eof)
            {
                m_pool.subscriber_connections.erase(this->shared_from_this());
                std::cout << "Connection closed, erasing subscriber connection\n";
            }
            else
            {
                boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&SubscriberConnection::handle_close, this->shared_from_this(), _1, _2));
            }
        }

        uint32_t m_msglen;
        std::vector<uint8_t> m_buffer;
        std::string m_node_name;
        SubscriberPool<SubscriberConnection<MsgType>> &m_pool;
    };

    template <class MsgType>
    class Publisher
    {
    public:
        Publisher(rosasio::Node &node, const std::string &topic_name)
            : m_node(node),
              m_topic_name(topic_name),
              m_acceptor(m_node.get_ioc(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0))
        {
            boost::asio::ip::tcp::endpoint le = m_acceptor.local_endpoint();
            m_node.register_publisher<MsgType>(topic_name, le.port());
            start_accept();
        }

        virtual ~Publisher()
        {
            m_node.unregister_publisher<MsgType>(m_topic_name);
        }

        void publish(const MsgType &msg)
        {
            for (auto conn : m_pool.subscriber_connections)
            {
                conn->publish(msg);
            }
        }

    private:
        void start_accept()
        {
            boost::asio::ip::tcp::socket sock(m_acceptor.get_executor());

            // TODO we don't really need to move here - just create the socket as part of the connection object amd pass in ref to ioc
            auto conn = std::make_shared<SubscriberConnection<MsgType>>(std::move(sock), m_node.get_name(), m_pool);

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
            }

            start_accept();
        }

        rosasio::Node &m_node;
        std::string m_topic_name;
        boost::asio::ip::tcp::acceptor m_acceptor;
        std::function<void(const MsgType &)> m_cb;
        SubscriberPool<SubscriberConnection<MsgType>> m_pool;
    };
} // namespace rosasio
