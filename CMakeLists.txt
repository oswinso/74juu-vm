cmake_minimum_required(VERSION 3.10)
project(vm C)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_STANDARD 11)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(vm main.c printing.c printing.h main.h)
target_link_libraries(vm ${CURSES_LIBRARIES})
add_executable(testing test.c)
target_link_libraries(testing ${CURSES_LIBRARIES})
