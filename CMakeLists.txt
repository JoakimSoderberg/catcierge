cmake_minimum_required(VERSION 2.6) 
project(catsnatch C) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenCV REQUIRED)

set(PROG_SRC main.c)
set(LIB_SRC catsnatch.c)

add_library(catsnatch ${LIB_SRC})
add_executable(catsnatch_tester ${PROG_SRC})

target_link_libraries(catsnatch ${OpenCV_LIBS})
add_dependencies(catsnatch_tester catsnatch)
target_link_libraries(catsnatch_tester catsnatch)
