cmake_minimum_required(VERSION 3.9)

# set project
project(candlelight-libusb)

# add custom module finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++17 standard by default
set(CMAKE_CXX_STANDARD 17)

find_package(LibUSB REQUIRED)

include_directories(${LIBUSB_INCLUDE_DIR})

add_subdirectory(src)
