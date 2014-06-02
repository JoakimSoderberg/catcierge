#
# Catcierge CMake project
# -----------------------
#
# Copyright (c) Joakim Soderberg 2013-2014
#
#  This file is part of Catcierge.
#
#    Catcierge is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 2 of the License, or
#    (at your option) any later version.
#
#    Foobar is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
#
cmake_minimum_required(VERSION 2.6) 
project(catcierge)

set(CATCIERGE_VERSION_MAJOR 0)
set(CATCIERGE_VERSION_MINOR 3)
set(CATCIERGE_VERSION_PATCH 0)

set(CATCIERGE_VERSION_STR "${CATCIERGE_VERSION_MAJOR}.${CATCIERGE_VERSION_MINOR}.${CATCIERGE_VERSION_PATCH}")

if (UNIX)
	option(CATCIERGE_WITH_MEMCHECK "Run tests with memory check using valgrind" ON)
	option(WITH_RFID "Build support for the serial port communication with RFID readers" ON)
	option(RPI "Build stuff for raspberry parts. Otherwise only the catcierge stuff is built." ON)
	option(CATCIERGE_COVERAGE "(GCC Only! Requires gcov/lcov to be installed). Include target for doing coverage analysis for the test suite. Note that -DCMAKE_BUILD_TYPE=Debug must be set" OFF)
	set(RPI_USERLAND "/home/pi/dev/userland" CACHE STRING "Path to rpi userland tools https://github.com/raspberrypi/userland.git")
endif()

# Set some nicer output dirs.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

if (CATCIERGE_COVERAGE)
	if(NOT CMAKE_COMPILER_IS_GNUCXX)
		if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
			message(FATAL_ERROR "Compiler is not GNU gcc! Aborting... You can set this on the command line using CC=/usr/bin/gcc CXX=/usr/bin/g++ cmake <options> ..")
		endif()
	endif()

	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	  message(FATAL_ERROR "Code coverage results with an optimised (non-Debug) build may be misleading! Add -DCMAKE_BUILD_TYPE=Debug")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

if (WIN32)
	set(WITH_RFID OFF)
	message("\n!!! Windows currently doesn't support RFID readers !!!\n")

	# Windows couldn't possibly be a raspberry pi...
	set(RPI OFF)
endif()

if (MSVC)
	# Turn off deprecation warnings
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
endif()

if (RPI)
	add_definitions(-DRPI)
endif()

#
# System introspection.
#
include(CheckIncludeFiles)
check_include_files(unistd.h CATCIERGE_HAVE_UNISTD_H)
check_include_files(fcntl.h CATCIERGE_HAVE_FCNTL_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/catcierge_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/catcierge_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (NOT RPI AND NOT OpenCV_DIR)
message("-----------------------------------------------------------------")
message("If CMake cannot find your OpenCV install automatically, you can set:")
message("")
message("-DOpenCV_DIR=<path>")
message("")
message("to point to the <path> where OpenCVConfig.cmake can be found.")
message("This file can be found in the build tree of OpenCV")
message("-----------------------------------------------------------------")
endif()

# Lazy solution for linking against OpenCV on Raspbian.
# Simply hard code the paths to the libs...
#
# This can be overridden by specifying your own OpenCV build
# using -DOpenCV_DIR=<path>
# Where <path> is the directory containing OpenCVConfig.cmake
# 
if (RPI AND NOT OpenCV_DIR)
	set(OpenCV_LIBS 
		/usr/lib/libopencv_calib3d.a
		/usr/lib/libopencv_features2d.a
		/usr/lib/libopencv_highgui.a
		/usr/lib/libopencv_ml.a
		/usr/lib/libopencv_contrib.a
		/usr/lib/libopencv_flann.a
		/usr/lib/libopencv_imgproc.a
		/usr/lib/libopencv_objdetect.a
		/usr/lib/libopencv_core.a
		/usr/lib/libopencv_gpu.a
		/usr/lib/libopencv_legacy.a
		/usr/lib/libopencv_video.a
		)

	set(OPENEXR_LIBRARIES
		/usr/lib/libIlmImf.a
		/usr/lib/libIlmImf.so
		/usr/lib/libHalf.so.6
		)

	set(JASPER_LIBRARIES
		/usr/lib/arm-linux-gnueabihf/libjasper.a
		/usr/lib/arm-linux-gnueabihf/libjasper.so
		/usr/lib/arm-linux-gnueabihf/libjasper.so.1
		)

	find_package(ZLIB)
	find_package(JPEG)
	find_package(TIFF)
	find_package(GTK2)

	set(LIBS 
		${JASPER_LIBRARIES}
		${OPENEXR_LIBRARIES}
		${GTK2_LIBRARIES}
		${TIFF_LIBRARIES}
		${JPEG_LIBRARIES}
		${ZLIB_LIBRARIES}
		${OpenCV_LIBS} 
		stdc++ 
		rt
		)

	include_directories(
		${RPI_USERLAND}/host_applications/linux/libs/bcm_host/include/
		${RPI_USERLAND}/host_applications/linux/apps/raspicam/
		${RPI_USERLAND}/interface/vcos/pthreads/
		${RPI_USERLAND}/interface/vmcs_host/linux/
		${RPI_USERLAND}/interface/mmal/
		${RPI_USERLAND}/interface/khronos/include/
		${RPI_USERLAND}
		raspicam_cv
		)

	# Raspicam lib.
	file(GLOB RASPICAM_SRC ${RPI_USERLAND}/host_applications/linux/apps/raspicam/*.c)
	list(APPEND RASPICAM_SRC raspicam_cv/RaspiCamCV.c)

	add_library(raspicamcv ${RASPICAM_SRC})

	set(RASPICAM_LIBS
		${RPI_USERLAND}/build/lib/libmmal_core.so
		${RPI_USERLAND}/build/lib/libmmal.so
		${RPI_USERLAND}/build/lib/libmmal_util.so
		${RPI_USERLAND}/build/lib/libvcos.so
		${RPI_USERLAND}/build/lib/libbcm_host.so
		)

	target_link_libraries(raspicamcv ${RASPICAM_LIBS})

	# Raspicam test app.
	set(RASPICAM_TEST_SRC raspicam_cv/RaspiCamTest.c)
	add_executable(raspicam_test ${RASPICAM_TEST_SRC})
	add_dependencies(raspicam_test raspicamcv)
	target_link_libraries(raspicam_test raspicamcv ${LIBS})
else()
	if (WIN32)
		# We link statically on windows so we don't have to copy DLLs around.
		option(LINK_STATIC "Link eveything static such as OpenCV. Will produce a single exe (big since it contains OpenCV and more). Default on" ON)

		if (LINK_STATIC)
			set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
			set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
			set(OpenCV_STATIC ON)
		endif()
	endif()

	find_package(OpenCV REQUIRED)
	set(LIBS ${OpenCV_LIBS})
endif()

# Catcierge lib.
set(LIB_SRC catcierge.c)

if (WITH_RFID)
	add_definitions(-DWITH_RFID)
	list(APPEND LIB_SRC catcierge_rfid.c)
endif()

list(APPEND LIB_SRC alini/alini.c)

add_library(catcierge ${LIB_SRC})
target_link_libraries(catcierge ${LIBS})

# Test program for the catcierge matcher.
set(PROG_SRC catcierge_tester.c)
add_executable(catcierge_tester ${PROG_SRC})
add_dependencies(catcierge_tester catcierge)
target_link_libraries(catcierge_tester catcierge)

# Test program for the catcierge rfid readers.
if (WITH_RFID)
	set(RFID_SRC 
		catcierge_rfid_tester.c 
		catcierge_rfid.c
		catcierge_log.c)
	add_executable(catcierge_rfid_tester ${RFID_SRC})
endif()

# Capture program for the catcierge.
set(GRABBER2_SRC 
		catcierge_grabber2.c 
		catcierge_args.c
		catcierge_log.c
		catcierge_util.c
		catcierge_timer.c
		catcierge_fsm.c)

if (RPI)
	list(APPEND GRABBER2_SRC catcierge_gpio.c)
endif()

if (WIN32)
	list(APPEND GRABBER2_SRC win32/gettimeofday.c)
endif()

add_executable(catcierge_grabber2 ${GRABBER2_SRC})
add_dependencies(catcierge_grabber2 catcierge)
target_link_libraries(catcierge_grabber2 catcierge)

if (RPI)
	add_dependencies(catcierge_grabber2 raspicamcv)
	target_link_libraries(catcierge_grabber2 raspicamcv)
endif()

################################## Tests ##################################
set(REGRESS_SRC
	${PROJECT_SOURCE_DIR}/catcierge_args.c
	${PROJECT_SOURCE_DIR}/catcierge_fsm.c
	${PROJECT_SOURCE_DIR}/catcierge_log.c
	${PROJECT_SOURCE_DIR}/catcierge_timer.c
	${PROJECT_SOURCE_DIR}/catcierge_util.c)

if (WITH_RFID)
	list(APPEND REGRESS_SRC ${PROJECT_SOURCE_DIR}/catcierge_rfid.c)
endif()

if (WIN32)
	list(APPEND REGRESS_SRC ${PROJECT_SOURCE_DIR}/win32/gettimeofday.c)
endif()

if (RPI)
	list(APPEND REGRESS_SRC ${PROJECT_SOURCE_DIR}/catcierge_gpio.c)
endif()

ENABLE_TESTING()
add_subdirectory(test)

message("------------------------------------------------------------------")
message(" Settings:")
message("------------------------------------------------------------------")
message("                                 Version: v${CATCIERGE_VERSION_STR}")
message("          Built for Raspberry pi (-DRPI): ${RPI}")
message("  Raspberry pi userland (-DRPI_USERLAND): ${RPI_USERLAND}")
message("              RFID support (-DWITH_RFID): ${WITH_RFID}")
message("          Run valgrind memcheck on tests ")
message("             (-DCATCIERGE_WITH_MEMCHECK): ${CATCIERGE_WITH_MEMCHECK}")
message("           Compile with coverage support")
message("                  (-DCATCIERGE_COVERAGE): ${CATCIERGE_COVERAGE}")
message("-----------------------------------------------------------------")
if (RPI)
message(" If compiling on another platform than raspberry pi, make sure ")
message(" you add -DRPI=OFF to the command line !!!")
message("-----------------------------------------------------------------")
endif()



