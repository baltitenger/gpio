cmake_minimum_required(VERSION 3.15)
project(gpio LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads)

add_library(gpio INTERFACE)
target_include_directories(gpio INTERFACE include)
target_link_libraries(gpio INTERFACE Threads::Threads)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # We are building as stand-alone project

	string(APPEND CMAKE_CXX_FLAGS_DEBUG " -Wall -Wextra -g -O0")

  add_executable(gpio_read src/read.cpp)
  target_link_libraries(gpio_read gpio)

  add_executable(gpio_write src/write.cpp)
  target_link_libraries(gpio_write gpio)

  add_executable(gpio_event src/event.cpp)
  target_link_libraries(gpio_event gpio)
endif ()
