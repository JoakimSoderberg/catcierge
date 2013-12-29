cmake_minimum_required(VERSION 2.6) 
project(catsnatch) 

option(WITH_RFID "Build support for the serial port communication with RFID readers" ON)
option(RPI "Build stuff for raspberry parts. Otherwise only the catsnatch stuff is built." ON)
set(RPI_USERLAND "/home/pi/dev/userland" CACHE STRING "Path to rpi userland tools https://github.com/raspberrypi/userland.git")

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

	message("${RPI_USERLAND}")
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
add_library(catsnatch ${LIB_SRC})
target_link_libraries(catsnatch ${LIBS})

# Test program for the catsnatch matcher.
set(PROG_SRC catsnatch_tester.c)
add_executable(catsnatch_tester ${PROG_SRC})
add_dependencies(catsnatch_tester catsnatch)
target_link_libraries(catsnatch_tester catsnatch)

# Capture program for the catsnacher.
set(GRABBER_SRC 
	catsnatch_grabber.c 
	catsnatch_gpio.c)
add_executable(catsnatch_grabber ${GRABBER_SRC})
add_dependencies(catsnatch_grabber catsnatch raspicamcv)
target_link_libraries(catsnatch_grabber catsnatch raspicamcv)


