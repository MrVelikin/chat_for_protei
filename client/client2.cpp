#include <deque>
#include <array>
#include <thread>
#include <iostream>
#include <cstring>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

using boost::asio::ip::tcp;

#ifndef PROTOCOL_HPP_
#define PROTOCOL_HPP_

enum : unsigned
{
	MAX_IP_PACK_SIZE = 1024,
	MAX_NICKNAME = 16,
	//PADDING = 24
};

#endif /* PROTOCOL_HPP_ */

void init()
{
    logging::add_file_log
    (
        keywords::file_name = "sample_%N.log",                                        /*< file name pattern >*/
        keywords::rotation_size = 10 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), /*< ...or at midnight >*/
        keywords::format = "[%TimeStamp%]: %Message%"                                 /*< log record format >*/
    );


}

class client
{
public:
    client(const std::array<char, MAX_NICKNAME>& nickname,
            boost::asio::io_service& io_service,
            tcp::resolver::iterator endpoint_iterator) : io_service_(io_service), socket_(io_service)
    {
        strcpy(nickname_.data(), nickname.data());
        memset(read_massagge.data(), '\0', MAX_IP_PACK_SIZE);
        boost::asio::async_connect(socket_, endpoint_iterator, boost::bind(&client::onConnect, this, _1));
    }

    void write(const std::array<char, MAX_IP_PACK_SIZE>& msg)
    {
        io_service_.post(boost::bind(&client::writeImpl, this, msg));
    }

    void close()
    {
        io_service_.post(boost::bind(&client::closeImpl, this));
    }

private:

    void onConnect(const boost::system::error_code& error)
    {
        if (!error)
        {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(nickname_, nickname_.size()),
                                     boost::bind(&client::readHandler, this, _1));
        }
    }

    void readHandler(const boost::system::error_code& error)
    {
        std::cout << read_massagge.data() << std::endl;
        if (!error)
        {
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_massagge, read_massagge.size()),
                                    boost::bind(&client::readHandler, this, _1));
        } else
        {
            closeImpl();
        }
    }

    void writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg)
    {
        bool write_in_progress = !write_massagge.empty();
        write_massagge.push_back(msg);
        if (!write_in_progress)
        {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(write_massagge.front(), write_massagge.front().size()),
                                     boost::bind(&client::writeHandler, this, _1));
        }
    }

    void writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            write_massagge.pop_front();
            if (!write_massagge.empty())
            {
                boost::asio::async_write(socket_,
                                         boost::asio::buffer(write_massagge.front(),write_massagge.front().size()),
                                         boost::bind(&client::writeHandler, this, _1));
            }
        } else
        {
            closeImpl();
        }
    }

    void closeImpl()
    {
        socket_.close();
    }

    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    std::array<char, MAX_IP_PACK_SIZE> read_massagge;
    std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_massagge;
    std::array<char, MAX_NICKNAME> nickname_;
};

int main(int argc, char* argv[])
{
    init();
    logging::add_common_attributes();

    using namespace logging::trivial;
    src::severity_logger< severity_level > lg;
    BOOST_LOG_SEV(lg, debug) << "Start of debug";
    try
    {
        if (argc != 3)
        {
            std::cout << "Используйте:client <ip хоста> <порт>\n";
            return 1;
        }
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(argv[1], argv[2]);
        tcp::resolver::iterator iterator = resolver.resolve(query);
        std::array<char, MAX_NICKNAME> nickname;
        strcpy(nickname.data(), argv[1]);

        client class_of_client( nickname,io_service, iterator);

        std::thread thread_of_client(boost::bind(&boost::asio::io_service::run, &io_service));

        std::array<char, MAX_IP_PACK_SIZE> massage_to_server;

        while (true)
        {
            memset(massage_to_server.data(), '\0', massage_to_server.size());
            if (!std::cin.getline(massage_to_server.data(), MAX_IP_PACK_SIZE - MAX_NICKNAME))
            {
                std::cin.clear(); //clean up error bit and try to finish reading
            }
            class_of_client.write(massage_to_server);
        }
        class_of_client.close();
        thread_of_client.join();
    } catch (std::exception& except)
    {
       BOOST_LOG_SEV(lg, error) << except.what() ;
    }

    return 0;
}
