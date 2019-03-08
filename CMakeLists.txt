cmake_minimum_required(VERSION 3.13)
project(Streaming)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -O0 -Wall ")
file(GLOB SRC_FILES "src/*.h" "src/*.cpp")
add_executable(Streaming ${SRC_FILES})

target_link_libraries(Streaming "-lavformat -lavcodec -lavutil -lswscale")
target_link_libraries(Streaming "-lflycapture -lpthread -lrt")