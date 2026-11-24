cmake_minimum_required(VERSION 3.15)

project(SugRedis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")

add_compile_options(-Wall -Wextra)

# Platform-specific poller backend
if(APPLE)
    set(POLLER_SRC src/poller_kqueue.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(POLLER_SRC src/poller_epoll.cpp)
elseif(CMAKE_SYSTEM_NAME MATCHES "^(FreeBSD|OpenBSD|NetBSD|DragonFly)$")
    set(POLLER_SRC src/poller_kqueue.cpp)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

add_executable(server src/server.cpp src/store.cpp ${POLLER_SRC})
target_include_directories(server PRIVATE src)

add_executable(client src/client.cpp)
target_include_directories(client PRIVATE src)
