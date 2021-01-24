#include <boost/asio.hpp>
#include <ros/service_traits.h>
#include <ros/serialization.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>
#include <boost/regex.hpp>

namespace rosasio
{
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
}