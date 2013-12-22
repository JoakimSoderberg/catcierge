cmake_minimum_required (VERSION 2.6) 
project (CurveMatching) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(OpenCV  REQUIRED)

message("OpenCV ${OpenCV_FOUND}")
add_executable(test test.c)

target_link_libraries(test ${OpenCV_LIBS})
