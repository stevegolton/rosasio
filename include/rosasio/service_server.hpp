#pragma once

#include <vector>
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

        void start()
        {
            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&ClientConnection::handle_read_header_length, this->shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read_header_length(boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                return;
            }

            m_buffer.resize(m_msglen);

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(m_buffer),
                                    std::bind(&ClientConnection::handle_read_header, this->shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
        }

        void handle_read_header(boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                return;
            }

            const std::string type = ros::service_traits::DataType<MsgType>::value();

            Message msg;
            msg.add_field("callerid", m_node_name);
            msg.add_field("md5sum", ros::service_traits::MD5Sum<MsgType>::value());
            msg.add_field("request_type", type + "Request");
            msg.add_field("response_type", type + "Response");
            msg.add_field("type", type);
            msg.finish();
            m_header = msg.buf;

            boost::asio::async_write(m_sock,
                                     boost::asio::buffer(m_header),
                                     std::bind(&ClientConnection::handle_write_header,
                                               this->shared_from_this(),
                                               std::placeholders::_1));
        }

        void handle_write_header(boost::system::error_code ec)
        {
            if (ec)
            {
                return;
            }

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&ClientConnection::handle_read_request_len, this->shared_from_this(),
                                              std::placeholders::_1));
        }

        void handle_read_request_len(boost::system::error_code ec)
        {
            if (ec)
            {
                return;
            }

            if (0 == m_msglen)
            {
                do_call();
            }
            else
            {
                m_buffer.resize(m_msglen);

                boost::asio::async_read(m_sock,
                                        boost::asio::buffer(m_buffer),
                                        std::bind(&ClientConnection::handle_read_request, this->shared_from_this(),
                                                  std::placeholders::_1));
            }
        }

        void do_call()
        {
            namespace ser = ros::serialization;

            typename MsgType::Request req;
            ser::IStream stream(m_buffer.data(), m_msglen);
            ser::deserialize(stream, req);

            typename MsgType::Response resp;
            auto success = m_cb(req, resp);

            {
                serial_size = ros::serialization::serializationLength(resp);
                m_write_buffer.reset(new uint8_t[serial_size]);

                ser::OStream stream(m_write_buffer.get(), serial_size);
                ser::serialize(stream, resp);

                res = success ? 0x01 : 0x00;
                std::vector<boost::asio::const_buffer> gather;
                gather.push_back(boost::asio::buffer(&res, sizeof(res)));
                gather.push_back(boost::asio::buffer(&serial_size, sizeof(serial_size)));
                gather.push_back(boost::asio::buffer(m_write_buffer.get(), serial_size));
                boost::asio::async_write(m_sock,
                                         gather,
                                         std::bind(&ClientConnection::handle_write,
                                                   this->shared_from_this(),
                                                   std::placeholders::_1));
            }
        }

        void handle_read_request(boost::system::error_code ec)
        {
            if (ec)
            {
                return;
            }

            do_call();
        }

        void handle_write(boost::system::error_code ec)
        {
            if (ec)
            {
                return;
            }

            boost::asio::async_read(m_sock,
                                    boost::asio::buffer(&m_msglen, sizeof(m_msglen)),
                                    std::bind(&ClientConnection::handle_read_request_len,
                                              this->shared_from_this(),
                                              std::placeholders::_1));
        }

        boost::asio::ip::tcp::socket m_sock;

    private:
        uint32_t m_msglen;
        std::vector<uint8_t> m_buffer;
        std::string m_node_name;
        std::string m_topic_name;
        CbType m_cb;

        boost::shared_array<uint8_t> m_write_buffer;
        uint32_t serial_size;
        char res;
        std::vector<uint8_t> m_header;
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
            boost::asio::ip::tcp::socket sock(m_node.get_ioc());

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