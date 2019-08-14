cmake_minimum_required(VERSION 3.15)
project(gpio LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(gpio INTERFACE)
target_include_directories(gpio INTERFACE include)
target_link_libraries(gpio INTERFACE boost_system pthread)

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
  # We are building as stand-alone project

  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(NOTICE "Build type not set, defaulting to ${CMAKE_BUILD_TYPE}.")
  endif ()

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g -Og")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

  add_executable(gpio_read src/read.cpp)
  target_link_libraries(gpio_read gpio)

  add_executable(gpio_write src/write.cpp)
  target_link_libraries(gpio_write gpio)

  add_executable(gpio_event src/event.cpp)
  target_link_libraries(gpio_event gpio)
endif()
