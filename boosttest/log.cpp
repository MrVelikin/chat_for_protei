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


int main(int, char*[])
{
    init();
    logging::add_common_attributes();

    using namespace logging::trivial;
    src::severity_logger< severity_level > lg;
    int b = 1243;
    char a[]="Hey";
    BOOST_LOG_SEV(lg, trace) << "A trace severity message";
    BOOST_LOG_SEV(lg, debug) << "A debug severity message";
    BOOST_LOG_SEV(lg, info) << "An informational severity message";
    BOOST_LOG_SEV(lg, warning) << "A warning severity message";
    BOOST_LOG_SEV(lg, error) << "An error severity message";
    BOOST_LOG_SEV(lg, fatal) << "A fatal severity message";
    BOOST_LOG(lg) << "Hello, World!";
    BOOST_LOG(lg) << "This is 1234 - "<< b;
    BOOST_LOG(lg) << "This is Hey - "<< a;
    BOOST_LOG_SEV(lg, info) << "An informational severity message";
    return 0;
}