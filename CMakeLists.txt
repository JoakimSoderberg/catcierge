#
# Catsnatch CMake project
# -----------------------
#
# Copyright (c) Joakim Soderberg 2013-2014
#
#  This file is part of Catsnatch.
#
#    Catsnatch is free software: you can redistribute it and/or modify
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
project(catsnatch)

set(CATSNATCH_VERSION_MAJOR 0)
set(CATSNATCH_VERSION_MINOR 1)
set(CATSNATCH_VERSION_PATCH 0)

set(CATSNATCH_VERSION_STR "${CATSNATCH_VERSION_MAJOR}.${CATSNATCH_VERSION_MINOR}.${CATSNATCH_VERSION_PATCH}")

option(WITH_INI "Build ini config file support" ON)
option(WITH_RFID "Build support for the serial port communication with RFID readers" ON)
option(RPI "Build stuff for raspberry parts. Otherwise only the catsnatch stuff is built." ON)
set(RPI_USERLAND "/home/pi/dev/userland" CACHE STRING "Path to rpi userland tools https://github.com/raspberrypi/userland.git")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/catsnatch_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/catsnatch_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (RPI)
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

# Catsnatch lib.
set(LIB_SRC catsnatch.c)

if (WITH_RFID)
	add_definitions(-DWITH_RFID)
	list(APPEND LIB_SRC catsnatch_rfid.c)
endif()

if (WITH_INI)
	add_definitions(-DWITH_INI)
	list(APPEND LIB_SRC alini/alini.c)
endif()

add_library(catsnatch ${LIB_SRC})
target_link_libraries(catsnatch ${LIBS})

# Test program for the catsnatch matcher.
set(PROG_SRC catsnatch_tester.c)
add_executable(catsnatch_tester ${PROG_SRC})
add_dependencies(catsnatch_tester catsnatch)
target_link_libraries(catsnatch_tester catsnatch)

# Test program for the catsnatch rfid readers.
if (WITH_RFID)
	set(RFID_SRC 
		catsnatch_rfid_tester.c 
		catsnatch_rfid.c
		catsnatch_log.c)
	add_executable(catsnatch_rfid_tester ${RFID_SRC})
endif()

# Capture program for the catsnacher.
if (RPI)
	set(GRABBER_SRC 
		catsnatch_grabber.c 
		catsnatch_gpio.c
		catsnatch_log.c
		catsnatch_util.c)
	add_executable(catsnatch_grabber ${GRABBER_SRC})
	add_dependencies(catsnatch_grabber catsnatch raspicamcv)
	target_link_libraries(catsnatch_grabber catsnatch raspicamcv)
endif()

message("------------------------------------------------------------------")
message(" Settings:")
message("------------------------------------------------------------------")
message("                                 Version: v${CATSNATCH_VERSION_STR}")
message("          Built for Raspberry pi (-DRPI): ${RPI}")
message("  Raspberry pi userland (-DRPI_USERLAND): ${RPI_USERLAND}")
message("         INI Config support (-DWITH_INI): ${WITH_INI}")
message("              RFID support (-DWITH_RFID): ${WITH_RFID}")
message("------------------------------------------------------------------")


