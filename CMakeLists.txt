cmake_minimum_required(VERSION 3.10)
project(find)

set(CMAKE_CXX_STANDARD 17)

add_executable(find os_finder.h os_finder.cpp main.cpp)

target_link_libraries(find stdc++fs)

find_package(Boost REQUIRED COMPONENTS program_options)

include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(find ${Boost_LIBRARIES})