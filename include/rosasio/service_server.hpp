#pragma once

#include <boost/asio.hpp>
#include <ros/service_traits.h>
#include <ros/serialization.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>
#include <boost/regex.hpp>
#include <rosasio/node.hpp>

namespace rosasio
{
    template <class MsgType>
    class ClientConnection : public std::enable_shared_from_this<ClientConnection<MsgType>>
    {
        typedef std::function<bool(const typename MsgType::Request &, typename MsgType::Response &)> CbType;

    public:
        ClientConnection(boost::asio::ip::tcp::socket &&sock, std::string node_name, std::string topic_name, CbType cb)
            : m_sock(std::move(sock)),
              m_node_name(node_name),
              m_topic_name(topic_name),
              m_cb(cb)
        {
        }

        // void publish(const MsgType &msg)
        // {
        //     if (!initialized)
        //         return;

        //     namespace ser = ros::serialization;

        //     uint32_t serial_size = ros::serialization::serializationLength(msg);
        //     boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

        //     ser::OStream stream(buffer.get(), serial_size);
        //     ser::serialize(stream, msg);

        //     try
        //     {

        //         boost::asio::write(m_sock, boost::asio::buffer(&serial_size, sizeof(serial_size)));
        //         boost::asio::write(m_sock, boost::asio::buffer(buffer.get(), serial_size));
        //     }
        //     catch (const boost::system::system_error &e)
        //     {
        //     }
        // }

        void start()
        {
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&ClientConnection::handle_read_header_length, this->shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read_header_length(boost::system::error_code ec, std::size_t bytes_read)
        {
            if (ec)
            {
                std::cout << "Read error\n";
                return;
            }

            std::cout << "Received header length: " << m_msglen << "\n";

            m_buffer.resize(m_msglen);

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(m_buffer),
                                    std::bind(&ClientConnection::handle_read_header, this->shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read_header(boost::system::error_code ec, std::size_t bytes_read)
        {
            if (ec)
            {
                std::cout << "Read error\n";
                return;
            }

            std::cout << "Received header\n";

            // TODO write async
            Message msg;
            // msg.add_field("message_definition=string-data\n\n");

            const std::string type = ros::service_traits::DataType<MsgType>::value();
            
            msg.add_field("callerid", m_node_name);
            msg.add_field("md5sum", ros::service_traits::MD5Sum<MsgType>::value());
            msg.add_field("request_type", type + "Request");
            msg.add_field("response_type", type + "Response");
            msg.add_field("type", type);
            msg.finish();
            boost::asio::write(m_sock, boost::asio::buffer(msg.buf));

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&ClientConnection::handle_read_request_len, this->shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read_request_len(boost::system::error_code ec, std::size_t bytes_read)
        {
            if (ec)
            {
                std::cout << "Read error\n";
                return;
            }

            std::cout << "Received request length: " << m_msglen << "\n";

            if (0 == m_msglen)
            {
                namespace ser = ros::serialization;

                typename MsgType::Request req;
                ser::IStream stream(m_buffer.data(), m_msglen);
                ser::deserialize(stream, req);

                typename MsgType::Response resp;
                auto success = m_cb(req, resp);

                {
                    uint32_t serial_size = ros::serialization::serializationLength(resp);
                    boost::shared_array<uint8_t> buffer(new uint8_t[serial_size]);

                    ser::OStream stream(buffer.get(), serial_size);
                    ser::serialize(stream, resp);

                    char res = success? 0x01 : 0x00;
                    boost::asio::write(m_sock, boost::asio::buffer(&res, sizeof(res)));
                    boost::asio::write(m_sock, boost::asio::buffer(&serial_size, sizeof(serial_size)));
                    boost::asio::write(m_sock, boost::asio::buffer(buffer.get(), serial_size));
                }
            }
            else
            {
                // TODO read the actual request properly!
            }
        }

        // oid on_message_length_received(boost::system::error_code ec, std::size_t len)
        // {
        //     buf.resize(msglen);
        //     boost::asio::async_read(m_sock,
        //                             boost::asio::buffer(buf),
        //                             std::bind(&PublisherConnection::on_message_received, this->shared_from_this(),
        //                                       std::placeholders::_1,
        //                                       std::placeholders::_2));
        // }

        // void on_message_received(boost::system::error_code ec, std::size_t len)
        // {
        //     buf.resize(msglen);
        //     boost::asio::async_read(m_sock,
        //                             boost::asio::buffer(&msglen, sizeof(msglen)),
        //                             std::bind(&PublisherConnection::on_message_length_received, this->shared_from_this(),
        //                                       std::placeholders::_1,
        //                                       std::placeholders::_2));

        //     namespace ser = ros::serialization;
        //     MsgType msg;
        //     ser::IStream stream(buf.data(), msglen);
        //     ser::deserialize(stream, msg);
        //     m_cb(msg);
        // }

        boost::asio::ip::tcp::socket m_sock;

    private:
        uint32_t m_msglen;
        std::vector<uint8_t> m_buffer;
        std::string m_node_name;
        std::string m_topic_name;
        CbType m_cb;
    };

    template <class MsgType>
    class ServiceServer
    {
        typedef std::function<bool(const typename MsgType::Request &, typename MsgType::Response &)> CbType;

    public:
        ServiceServer(rosasio::Node &node, const std::string &service_name, CbType cb)
            : m_node(node),
              m_service_name(service_name),
              m_cb(cb),
              m_acceptor(m_node.get_ioc(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0))
        {
            boost::asio::ip::tcp::endpoint le = m_acceptor.local_endpoint();
            m_node.register_service<MsgType>(m_service_name, le.port());

            start_accept();
        }

        virtual ~ServiceServer()
        {
            boost::asio::ip::tcp::endpoint le = m_acceptor.local_endpoint();
            m_node.unregister_service<MsgType>(m_service_name, le.port());
        }

    private:
        void start_accept()
        {
            boost::asio::ip::tcp::socket sock(m_acceptor.get_executor());

            // TODO we don't really need to move here - just create the socket as part of the connection object amd pass in ref to ioc
            auto conn = std::make_shared<ClientConnection<MsgType>>(std::move(sock), m_node.get_name(), m_service_name, m_cb);

            m_acceptor.async_accept(conn->m_sock,
                                    std::bind(
                                        &ServiceServer::accept_handler, this,
                                        conn,
                                        std::placeholders::_1));
        }

        void accept_handler(std::shared_ptr<ClientConnection<MsgType>> conn, const boost::system::error_code &error)
        {
            if (!error)
            {
                std::cout << "Accepted connection\n";
                conn->start();
            }

            start_accept();
        }

        rosasio::Node &m_node;
        std::string m_service_name;
        CbType m_cb;
        boost::asio::ip::tcp::acceptor m_acceptor;
    };
}