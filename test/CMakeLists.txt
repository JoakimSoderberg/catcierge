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

# Test drivers.
add_executable(${CATCIERGE_TEST_DRIVER}
				${CATCIERGE_TESTS_SRCS}
				catcierge_test_helpers.c
				${PROJECT_SOURCE_DIR}/catcierge_timer.c)

target_link_libraries(${CATCIERGE_TEST_DRIVER} catcierge)

foreach (test_name ${TEST_SRCS_NO_EXT})
	# Add a test with the filename we just got
	# And call the driver with the test as an argument.
	if (NOT CATCIERGE_WITH_MEMCHECK)
		add_test(${test_name} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CATCIERGE_TEST_DRIVER} ${test_name})
	else()
		add_test(${test_name}_memchk ${VALGRIND_PROGRAM} --error-exitcode=5 --leak-check=full ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CATCIERGE_TEST_DRIVER} ${test_name})
	endif()
endforeach()
