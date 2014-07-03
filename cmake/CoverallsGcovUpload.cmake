#
# This is intended to be run by a custom target in a CMake project like this.
# 0. Compile program with coverage support.
# 1. Clear coverage data. (Recursively delete *.gcda in build dir)
# 2. Run the unit tests.
# 3. Run this script specifying which source files the coverage should be performed on.
#
# This script will then use gcov to generate .gcov files in the directory specified
# via the COV_PATH var. This should probably be the same as your cmake build dir.
#
# It then parses the .gcov files to convert them into the Coveralls JSON format:
# https://coveralls.io/docs/api
#
# Example for running as standalone CMake script from the command line:
# (Not it is important the -P is at the end...)
# $ cmake -DCOV_PATH=$(pwd) 
#         -DCOVERAGE_SRCS="catcierge_rfid.c;catcierge_timer.c" 
#         -P ../cmake/CoverallsGcovUpload.cmake
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)


#
# Make sure we have the needed arguments.
#
if (NOT COV_PATH)
	message(FATAL_ERROR "Missing coverage directory path containing gcov files. Please set COV_PATH")
endif()

# TODO: Require these to be absolute path?
if (NOT COVERAGE_SRCS)
	message(FATAL_ERROR "Missing the list of source files that we should get the coverage data for COVERAGE_SRCS")
endif()

find_program(GCOV_EXECUTABLE gcov)

if (NOT GCOV_EXECUTABLE)
	message(FATAL_ERROR "gcov not found! Aborting...")
endif()

# Get the coverage data.
file(GLOB_RECURSE GCDA_FILES "${COV_PATH}/*.gcda")

# Get a list of all the object directories needed by gcov
# (The directories the .gcda files and .o files are found in)
#set(OBJECT_DIRS "")
foreach(GCDA ${GCDA_FILES})
	get_filename_component(GCDA_DIR ${GCDA} DIRECTORY)

	execute_process(
		COMMAND ${GCOV_EXECUTABLE} -o ${GCDA_DIR} ${GCDA}
		WORKING_DIRECTORY ${COV_PATH}
	)
endforeach()

# TODO: Make these be absolute path
file(GLOB ALL_GCOV_FILES ${COV_PATH}/*.gcov)

# Filter out all but the gcov files we want.
set(GCOV_FILES "")
foreach (GCOV_FILE ${ALL_GCOV_FILES})
	#message("gcov file: ${GCOV_FILE}")

	# Get rid of .gcov and path info.
	get_filename_component(GCOV_FILE ${GCOV_FILE} NAME)
	string(REGEX REPLACE "\\.gcov$" "" GCOV_FILE_WEXT ${GCOV_FILE})
	#message("gcov file wext: ${GCOV_FILE_WEXT}")

	# Is this in the list of source files?
	list(FIND COVERAGE_SRCS ${GCOV_FILE_WEXT} WAS_FOUND)
	
	if (NOT WAS_FOUND EQUAL -1)
		#message("Found!")
		list(APPEND GCOV_FILES ${GCOV_FILE})
	endif() 
endforeach()

message("Gcov files we want: ${GCOV_FILES}")

set(SRC_FILE_TEMPLATE
"{
  \"name\": \"\@GCOV_FILE\@\",
  \"source\": \"\@GCOV_FILE_SOURCE\@\",
  \"coverage\": \@GCOV_FILE_COVERAGE\@
}"
)

# Read the GCOV files line by line and get the coverage data.
foreach (GCOV_FILE ${GCOV_FILES})

	# Loads the gcov file as a list of lines.
	file(STRINGS ${GCOV_FILE} GCOV_LINES)

	# We want an array of coverage data and the
	# source as a single string with line endings as \n
	# start building them from the contents of the .gcov
	set(GCOV_FILE_COVERAGE "[")
	set(GCOV_FILE_SOURCE "")

	foreach (GCOV_LINE ${GCOV_LINES})
		# Example of what we're parsing:
		# Hitcount  |Line | Source
		# "        8:   26:        if (!allowed || (strlen(allowed) == 0))"
		string(REGEX REPLACE 
			"^([^:]*):([^:]*):(.*)$" 
			"\\1;\\2;\\3"
			RES
			"${GCOV_LINE}")

		list(GET RES 0 HITCOUNT)
		list(GET RES 1 LINE)
		list(GET RES 2 SOURCE)

		string(STRIP ${HITCOUNT} HITCOUNT)
		string(STRIP ${LINE} LINE)

		# Lines with 0 line numbers are metadata and can be ignored.
		if (NOT ${LINE} EQUAL 0)
			
			# Translate the hitcount into valid JSON values.
			if (${HITCOUNT} STREQUAL "#####")
				set(GCOV_FILE_COVERAGE "${GCOV_FILE_COVERAGE}0, ")
			elseif (${HITCOUNT} STREQUAL "-")
				set(GCOV_FILE_COVERAGE "${GCOV_FILE_COVERAGE}null, ")
			else()
				set(GCOV_FILE_COVERAGE "${GCOV_FILE_COVERAGE}${HITCOUNT}, ")
			endif()

			# Replace line ending literals (\n) in printf strings and such with
			# escaped versions (\\n). Coveralls uses \n for line endings.
			# Also escape " chars so we don't produce invalid JSON.
			string(REPLACE "\\n" "\\\\n" SOURCE "${SOURCE}")
			string(REGEX REPLACE "\"" "\\\\\"" SOURCE "${SOURCE}")

			# Concatenate all the sources into one long string that can
			# be put in the JSON file.
			# (Below is CMake 3.0.0 only. I guess it's faster....)
			#string(CONCAT GCOV_FILE_SOURCE "${SOURCE}\\n") 
			set(GCOV_FILE_SOURCE "${GCOV_FILE_SOURCE}${SOURCE}\\n")
		endif()
	endforeach()

	# Advanced way of removing the trailing comma in the JSON array.
	# "[1, 2, 3, " -> "[1, 2, 3"
	string(REGEX REPLACE ",[ ]*$" "" GCOV_FILE_COVERAGE ${GCOV_FILE_COVERAGE})

	# Append the trailing ] to complete the JSON array.
	set(GCOV_FILE_COVERAGE "${GCOV_FILE_COVERAGE}]")

	# Generate the final JSON for this file.
	string(CONFIGURE ${SRC_FILE_TEMPLATE} FILE_JSON)
	message("${FILE_JSON}")

endforeach()

message("${SRC_FILE_TEMPLATE}")

