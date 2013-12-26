cmake_minimum_required(VERSION 2.6) 
project(catsnatch) 

option(RPI "Build for raspberry pi" ON)

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
		rt)
else()
	find_package(OpenCV REQUIRED)
	set(LIBS ${OpenCV_LIBS})
endif()

message("${OpenCV_LIBS}")

set(PROG_SRC main.c)
set(LIB_SRC catsnatch.c)

add_library(catsnatch ${LIB_SRC})
add_executable(catsnatch_tester ${PROG_SRC})

target_link_libraries(catsnatch ${LIBS})
add_dependencies(catsnatch_tester catsnatch)
target_link_libraries(catsnatch_tester catsnatch)

