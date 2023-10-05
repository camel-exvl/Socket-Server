cmake_minimum_required(VERSION 3.26)
project(Lab5)

set(CMAKE_CXX_STANDARD 14)

add_executable(Lab5
        server.h
        server.cpp
        package.h
        package.cpp
)

add_executable(TestClient package.h package.cpp TestClient/TestClient.cpp)

if (WIN32)
    target_link_libraries(Lab5 wsock32 ws2_32)
    target_link_libraries(TestClient wsock32 ws2_32)
endif ()
