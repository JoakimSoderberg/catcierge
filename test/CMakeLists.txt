set(CATCIERGE_TEST_DRIVER catcierge_regress)

file(GLOB TEST_SRCS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/TEST_*.c")

create_test_sourcelist(
	CATCIERGE_TESTS_SRCS		# Name of the test suite.
	${CATCIERGE_TEST_DRIVER}.c	# Name of the test DRIVER source generated.

	# List of test files. All TEST_*.c files.
	${TEST_SRCS}
	)

#list(APPEND TEST_DRIVERS ${CATCIERGE_TEST_DRIVER}.c)

# Get the TEST_*.c filenames without extension.
set(TEST_SRCS_NO_EXT)
foreach (test ${TEST_SRCS})
	get_filename_component(TName ${test} NAME_WE)
	list(APPEND TEST_SRCS_NO_EXT ${TName})
endforeach()

include_directories("${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

set(CATCIERGE_IMG_ROOT "${PROJECT_SOURCE_DIR}/examples")
set(CATCIERGE_SNOUT1_PATH "${CATCIERGE_IMG_ROOT}/snout/snout320x240.png")
set(CATCIERGE_SNOUT2_PATH "${CATCIERGE_IMG_ROOT}/snout/snout320x240_04b.png")
configure_file(catcierge_test_config.h.in ${PROJECT_BINARY_DIR}/catcierge_test_config.h)

# Test drivers.
add_executable(${CATCIERGE_TEST_DRIVER}
				${CATCIERGE_TESTS_SRCS}
				catcierge_test_helpers.c
				${REGRESS_SRC})

target_link_libraries(${CATCIERGE_TEST_DRIVER} catcierge)

if (RPI)
	target_link_libraries(${CATCIERGE_TEST_DRIVER} raspicamcv)
endif()

if (CATCIERGE_WITH_MEMCHECK AND UNIX)
	find_program(VALGRIND_PROGRAM NAMES valgrind)

	if (NOT EXISTS ${VALGRIND_PROGRAM})
		message(WARNING "Valgrind not found! Tests will be run without memory check. Turn this warning off by installing valgrind or settings -DCATCIERGE_WITH_MEMCHECK=0")
		set(CATCIERGE_WITH_MEMCHECK 0)
	else()
		message("Found valgrind: ${VALGRIND_PROGRAM}")
	endif()	
endif()

foreach (test_name ${TEST_SRCS_NO_EXT})
	# Add a test with the filename we just got
	# And call the driver with the test as an argument.
	if (NOT CATCIERGE_WITH_MEMCHECK)
		add_test(${test_name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CATCIERGE_TEST_DRIVER} ${test_name})
	else()
		add_test(${test_name}_memchk ${VALGRIND_PROGRAM} --error-exitcode=5 --leak-check=full --suppressions=${PROJECT_SOURCE_DIR}/valgrind-supress.txt ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CATCIERGE_TEST_DRIVER} ${test_name})
	endif()
endforeach()
