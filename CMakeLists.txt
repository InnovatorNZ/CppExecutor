cmake_minimum_required(VERSION 3.18)
project(CppExecutor)
set(CMAKE_CXX_STANDARD 17)

include_directories(.)
aux_source_directory("." SRCS)
add_executable(CppExecutor ${SRCS} ${HDRS})