cmake_minimum_required(VERSION 3.12)

project(runner)

set(CMAKE_C_FLAGS
    "-g -Wall -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer")

aux_source_directory(. SRCS)
add_definitions(-DNDEBUG=1)

add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} asan pthread)
