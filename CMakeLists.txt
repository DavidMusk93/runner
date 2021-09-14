cmake_minimum_required(VERSION 2.8)

project(runner)

set(CMAKE_C_FLAGS "-g -Wall")

aux_source_directory(. SRCS)

add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} pthread)
