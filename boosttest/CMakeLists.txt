cmake_minimum_required(VERSION 3.16)
project(log)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -lboost_log -lpthread")
set(Boost_DEBUG 1)
set(CMAKE_CXX_STANDARD 14)
SET(Boost_USE_STATIC_LIBS ON)

find_package(Boost COMPONENTS thread log log_setup REQUIRED)

message(STATUS Boost_LIBRARIES:)
message (STATUS ${Boost_LIBRARIES})
message(STATUS BOOST_INCLUDEDIR:)
message(STATUS ${BOOST_INCLUDEDIR})

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()


add_executable(log log.cpp)
target_link_libraries(log pthread Boost::log Boost::log_setup )
