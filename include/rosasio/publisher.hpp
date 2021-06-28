#pragma once

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <rosasio/node.hpp>

#include <ros/message_traits.h>
#include <ros/serialization.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

namespace rosasio
{
    template <class MsgType>
    class SubscriberConnectonBase : public std::enable_shared_from_this<SubscriberConnectonBase<MsgType>>
    {
    public:
        virtual void close() = 0;
        virtual void publish(const MsgType &msg) = 0;
    };

    template <class MsgType>
    class SubscriberPool
    {
    public:
        SubscriberPool(bool latched)
            : m_latched(latched),
              m_has_latched_msg(false),
              open(true) {}

        void close()
        {
            for (auto conn : subscriber_connections)
            {
                conn->close();
            }
            open = false;
        }

        void publish(const MsgType &msg)
        {
            for (auto conn : subscriber_connections)
            {
                conn->publish(msg);
            }

            if (m_latched)
            {
                m_latched_msg = msg;
                m_has_latched_msg = true;
            }
        }

        std::set<std::shared_ptr<SubscriberConnectonBase<MsgType>>> subscriber_connections;
        bool m_latched;
        bool m_has_latched_msg;
        bool open;
        MsgType m_latched_msg;
    };

    template <class Protocol, class MsgType>
    class SubscriberConnection : public SubscriberConnectonBase<MsgType>
    {
    public:
        SubscriberConnection(boost::asio::basic_stream_socket<Protocol> &&sock,
                             std::string node_name,
                             std::weak_ptr<SubscriberPool<MsgType>> pool)
            : m_sock(std::move(sock)),
              m_node_name(node_name),
              m_pool(pool)
        {
        }

        void start()
        {
            // boost::asio::ip::tcp::no_delay option(true);
            // m_sock.set_option(option);

            auto shared_this = std::dynamic_pointer_cast<SubscriberConnection<Protocol, MsgType>>(this->shared_from_this());

            using namespace std::placeholders;
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&SubscriberConnection::handle_read_len, shared_this, _1, _2));
        }

        void publish(const MsgType &msg) override
        {
            namespace ser = ros::serialization;

            uint32_t serial_size = ros::serialization::serializationLength(msg);
            boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

            ser::OStream stream(buffer.get(), serial_size);
            ser::serialize(stream, msg);

            try
            {
                std::vector<boost::asio::const_buffer> gather;
                gather.push_back(boost::asio::buffer(&serial_size, sizeof(serial_size)));
                gather.push_back(boost::asio::buffer(buffer.get(), serial_size));
                boost::asio::write(m_sock, gather);
            }
            catch (const boost::system::system_error &e)
            {
            }
        }

        void close() override
        {
            m_sock.close();
        }

        boost::asio::basic_stream_socket<Protocol> m_sock;

    private:
        void handle_read_len(boost::system::error_code ec, std::size_t)
        {
            using namespace std::placeholders;

            if (ec)
                return;

            m_buffer.reserve(m_msglen);

            auto shared_this = std::dynamic_pointer_cast<SubscriberConnection<Protocol, MsgType>>(this->shared_from_this());
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(m_buffer),
                                    std::bind(&SubscriberConnection::handle_read, shared_this, _1, _2));
        }

        void handle_read(boost::system::error_code ec, std::size_t)
        {
            if (ec)
                return;

            if (auto pool = m_pool.lock())
            {
                using namespace std::placeholders;

                Message msg;
                msg.add_field("message_definition=string-data\n\n");
                msg.add_field("callerid", m_node_name);
                msg.add_field("md5sum", ros::message_traits::MD5Sum<MsgType>::value());
                msg.add_field("type", ros::message_traits::DataType<MsgType>::value());
                msg.add_field("latching", pool->m_latched ? "1" : "0");
                msg.finish();

                boost::asio::write(m_sock, boost::asio::buffer(msg.buf));

                std::cout << "Adding subscriber connection\n";
                if (pool->open)
                {
                    pool->subscriber_connections.insert(this->shared_from_this());

                    // TODO only publish lateched message if we actually have one to publish!
                    if (pool->m_latched && pool->m_has_latched_msg)
                    {
                        publish(pool->m_latched_msg);
                    }

                    // Start another read
                    auto shared_this = std::dynamic_pointer_cast<SubscriberConnection<Protocol, MsgType>>(this->shared_from_this());

                    m_sock.async_read_some(
                        boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                        std::bind(
                            &SubscriberConnection::handle_close,
                            shared_this, _1, _2));
                }
            }
        }

        void handle_close(boost::system::error_code ec, std::size_t bytes_read)
        {
            using namespace std::placeholders;

            if (ec == boost::asio::error::eof || bytes_read == 0)
            {
                if (auto pool = m_pool.lock())
                {
                    std::cout << "Connection closed, erasing subscriber connection\n";
                    pool->subscriber_connections.erase(this->shared_from_this());
                }
            }
            else
            {
                auto shared_this = std::dynamic_pointer_cast<SubscriberConnection<Protocol, MsgType>>(this->shared_from_this());
                m_sock.async_read_some(
                        boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                        std::bind(
                            &SubscriberConnection::handle_close,
                            shared_this, _1, _2));
            }
        }

        uint32_t m_msglen;
        std::vector<uint8_t> m_buffer;
        std::string m_node_name;
        std::weak_ptr<SubscriberPool<MsgType>> m_pool;
    };

    template <class MsgType>
    class Publisher
    {
    public:
        Publisher(rosasio::Node &node, const std::string &topic_name, bool latched = false)
            : m_node(node),
              m_topic_name(topic_name),
              m_tcp_acceptor(m_node.get_ioc(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0)),
              m_uds_acceptor(m_node.get_ioc(), boost::asio::local::stream_protocol::endpoint(gen_temp_file()))
        {
            m_node.register_publisher<MsgType>(
                topic_name,
                m_tcp_acceptor.local_endpoint().port(),
                m_uds_acceptor.local_endpoint().path());

            m_pool = std::make_shared<SubscriberPool<MsgType>>(latched);

            start_accept<boost::asio::ip::tcp>(m_tcp_acceptor);
            start_accept<boost::asio::local::stream_protocol>(m_uds_acceptor);
        }

        virtual ~Publisher()
        {
            m_node.unregister_publisher<MsgType>(m_topic_name);
            m_pool->close();
            ::unlink(m_uds_acceptor.local_endpoint().path().c_str());
        }

        virtual void publish(const MsgType &msg)
        {
            m_pool->publish(msg);
        }

        static std::string gen_temp_file()
        {
            using namespace boost::filesystem;
            path temp = "/tmp" / unique_path();
            return temp.native();
        }

    private:
        template <typename Protocol>
        void start_accept(boost::asio::basic_socket_acceptor<Protocol> &acceptor)
        {
            boost::asio::basic_stream_socket<Protocol> sock(m_node.get_ioc());

            // TODO we don't really need to move here - just create the socket as part of the connection object amd pass in ref to ioc
            auto conn = std::make_shared<SubscriberConnection<Protocol, MsgType>>(std::move(sock), m_node.get_name(), m_pool);

            acceptor.async_accept(
                conn->m_sock,
                std::bind(
                    &Publisher::accept_handler<Protocol>, this,
                    std::ref(acceptor),
                    conn,
                    std::placeholders::_1));
        }

        template <typename Protocol>
        void accept_handler(boost::asio::basic_socket_acceptor<Protocol> &acceptor, std::shared_ptr<SubscriberConnection<Protocol, MsgType>> conn, const boost::system::error_code &error)
        {
            if (!error)
            {
                conn->start();
            }

            start_accept<Protocol>(acceptor);
        }

        rosasio::Node &m_node;
        std::string m_topic_name;
        boost::asio::ip::tcp::acceptor m_tcp_acceptor;
        boost::asio::local::stream_protocol::acceptor m_uds_acceptor;
        std::function<void(const MsgType &)> m_cb;
        std::shared_ptr<SubscriberPool<MsgType>> m_pool;
    };
} // namespace rosasio
