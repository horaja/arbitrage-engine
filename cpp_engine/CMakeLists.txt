cmake_minimum_required(VERSION 3.18)

project(ArbitrageEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Eigen3)
# TODO: find_package(websocketpp REQUIRED)


add_executable(arbitrage_engine main.cpp arbitragegraph.cpp)
target_include_directories(arbitrage_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/libs
  ${Boost_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIR}
  # ${WEBSOCKETPP_INCLUDE_DIR}
)

target_link_libraries(arbitrage_engine PRIVATE
  Boost::system
  Boost::thread
)