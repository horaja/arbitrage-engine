cmake_minimum_required(VERSION 3.18)

project(ArbitrageEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Boost REQUIRED CONFIG)


add_executable(arbitrage_engine main.cpp arbitragegraph.cpp)
target_include_directories(arbitrage_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs)
target_link_libraries(arbitrage_engine PRIVATE Boost::boost)