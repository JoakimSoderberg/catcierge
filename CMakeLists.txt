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
set(CATCIERGE_VERSION_MINOR 1)
set(CATCIERGE_VERSION_PATCH 0)

set(CATCIERGE_VERSION_STR "${CATCIERGE_VERSION_MAJOR}.${CATCIERGE_VERSION_MINOR}.${CATCIERGE_VERSION_PATCH}")

option(WITH_INI "Build ini config file support" ON)
option(WITH_RFID "Build support for the serial port communication with RFID readers" ON)
option(RPI "Build stuff for raspberry parts. Otherwise only the catcierge stuff is built." ON)
set(RPI_USERLAND "/home/pi/dev/userland" CACHE STRING "Path to rpi userland tools https://github.com/raspberrypi/userland.git")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/catcierge_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/catcierge_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (RPI)
	add_definitions(-DRPI)

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
	find_package(OpenCV REQUIRED)
	set(LIBS ${OpenCV_LIBS})
endif()

# Catcierge lib.
set(LIB_SRC catcierge.c)

if (WITH_RFID)
	add_definitions(-DWITH_RFID)
	list(APPEND LIB_SRC catcierge_rfid.c)
endif()

if (WITH_INI)
	add_definitions(-DWITH_INI)
	list(APPEND LIB_SRC alini/alini.c)
endif()

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
set(GRABBER_SRC 
		catcierge_grabber.c 
		catcierge_log.c
		catcierge_util.c)

if (RPI)
	list(APPEND GRABBER_SRC catcierge_gpio.c)
endif()

add_executable(catcierge_grabber ${GRABBER_SRC})
add_dependencies(catcierge_grabber catcierge)
target_link_libraries(catcierge_grabber catcierge)

if (RPI)
	add_dependencies(catcierge_grabber raspicamcv)
	target_link_libraries(catcierge_grabber raspicamcv)
endif()

message("------------------------------------------------------------------")
message(" Settings:")
message("------------------------------------------------------------------")
message("                                 Version: v${CATCIERGE_VERSION_STR}")
message("          Built for Raspberry pi (-DRPI): ${RPI}")
message("  Raspberry pi userland (-DRPI_USERLAND): ${RPI_USERLAND}")
message("         INI Config support (-DWITH_INI): ${WITH_INI}")
message("              RFID support (-DWITH_RFID): ${WITH_RFID}")
message("------------------------------------------------------------------")

