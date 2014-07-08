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
set(CATCIERGE_CASCADE "${PROJECT_SOURCE_DIR}/extra/catcierge.xml")
configure_file(catcierge_test_config.h.in ${PROJECT_BINARY_DIR}/catcierge_test_config.h)

# Test drivers.
add_executable(${CATCIERGE_TEST_DRIVER}
				${CATCIERGE_TESTS_SRCS}
				catcierge_test_helpers.c
				catcierge_test_common.c
				${REGRESS_SRC})

target_link_libraries(${CATCIERGE_TEST_DRIVER} catcierge)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	# For openpty in the rfid tests.
	find_library(LIBUTIL util)
	target_link_libraries(${CATCIERGE_TEST_DRIVER} ${LIBUTIL})
endif()

if (RPI)
	target_link_libraries(${CATCIERGE_TEST_DRIVER} raspicamcv)
endif()

if (CATCIERGE_COVERAGE)
	if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		include(CodeCoverage)
		setup_target_for_coverage(
			${CATCIERGE_TEST_DRIVER}_coverage 	# Coverage make target.
			coverage 						# Name of output directory.
			make							# Name of test runner executable.
			test)							# Arguments to the test runner above (make test).
	endif()
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

###################### Coveralls!
# TODO: Move all this to it's own file.

# Check if we're running on travis to add this target.
if ($ENV{TRAVIS})
	set(COVERAGE_SRCS ${REGRESS_SRC})
	set(COVERALLS_FILE ${PROJECT_BINARY_DIR}/coveralls.json)

	find_program(CURL_EXECUTABLE curl)

	if (NOT CURL_EXECUTABLE)
		message(FATAL_ERROR "curl not found! Aborting")
	endif()

	add_custom_target(coveralls

		# Zero the coverage counters.
		COMMAND ${CMAKE_COMMAND}
				-P "${PROJECT_SOURCE_DIR}/cmake/CoverallsClear.cmake"

		# Run regress tests.
		COMMAND ${CMAKE_CTEST_COMMAND}

		# Generate Gcov and translate it into coveralls JSON.
		COMMAND ${CMAKE_COMMAND}
				-DCOVERAGE_SRCS="${COVERAGE_SRCS}"
				-DCOVERALLS_OUTPUT_FILE="${COVERALLS_FILE}"
				-DCOV_PATH="${PROJECT_BINARY_DIR}"
				-P "${PROJECT_SOURCE_DIR}/cmake/CoverallsGenerateGcov.cmake"

		# Upload the JSON to coveralls.
		COMMAND ${CURL_EXECUTABLE}
				-F json_file=@${COVERALLS_FILE}
				https://coveralls.io/api/v1/jobs

		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		COMMENT "Generating and uploading coveralls output..."
		)
else()
	message("Skipping coveralls upload, not running Travis")
endif()
