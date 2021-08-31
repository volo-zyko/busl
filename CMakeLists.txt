cmake_minimum_required(VERSION 3.0)

project(busl)

add_library(busllib busllib.c)

add_executable(busl busl.c)

target_link_libraries(busl busllib)
