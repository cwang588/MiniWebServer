cmake_minimum_required(VERSION 3.23)
project(MiniWebServer)

set(CMAKE_CXX_STANDARD 20)

add_executable(MiniWebServer main.cpp header/locker.h header/http.h source/http.cpp source/locker.cpp header/threadpool.h source/threadpool.cpp header/connectionpool.h source/connectionpool.cpp)
