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
if (NOT COVERALLS_OUTPUT_FILE)
	message(FATAL_ERROR "No coveralls output file specified. Please set COVERALLS_OUTPUT_FILE")
endif()

if (NOT COV_PATH)
	message(FATAL_ERROR "Missing coverage directory path where gcov files will be generated. Please set COV_PATH")
endif()

if (NOT COVERAGE_SRCS)
	message(FATAL_ERROR "Missing the list of source files that we should get the coverage data for COVERAGE_SRCS")
endif()

# Since it's not possible to pass a CMake list properly in the
# "1;2;3" format to an external process, we have replaced the
# ";" with "*", so reverse that here so we get it back into the
# CMake list format.
string(REGEX REPLACE "\\*" ";" COVERAGE_SRCS ${COVERAGE_SRCS})

find_program(GCOV_EXECUTABLE gcov)

if (NOT GCOV_EXECUTABLE)
	message(FATAL_ERROR "gcov not found! Aborting...")
endif()

find_package(Git)

# TODO: Add these git things to the coveralls json.
if (GIT_FOUND)
	# Branch.
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	macro (git_log_format FORMAT_CHARS VAR_NAME)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%${FORMAT_CHARS}
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE ${VAR_NAME}
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	endmacro()

	git_log_format(an GIT_AUTHOR_EMAIL)
	git_log_format(ae GIT_AUTHOR_EMAIL)
	git_log_format(cn GIT_COMMITTER_NAME)
	git_log_format(ce GIT_COMMITTER_EMAIL)
	git_log_format(B GIT_COMMIT_MESSAGE)

	message("Git exe: ${GIT_EXECUTABLE}")
	message("Git branch: ${GIT_BRANCH}")
	message("Git author: ${GIT_AUTHOR_NAME}")
	message("Git e-mail: ${GIT_AUTHOR_EMAIL}")
	message("Git commiter name: ${GIT_COMMITTER_NAME}")
	message("Git commiter e-mail: ${GIT_COMMITTER_EMAIL}")
	message("Git commit message: ${GIT_COMMIT_MESSAGE}")

endif()

# Get the coverage data.
file(GLOB_RECURSE GCDA_FILES "${COV_PATH}/*.gcda")

# Get a list of all the object directories needed by gcov
# (The directories the .gcda files and .o files are found in)
# and run gcov on those.
foreach(GCDA ${GCDA_FILES})
	get_filename_component(GCDA_DIR ${GCDA} PATH)

	execute_process(
		COMMAND ${GCOV_EXECUTABLE} -o ${GCDA_DIR} ${GCDA}
		WORKING_DIRECTORY ${COV_PATH}
	)
endforeach()

# TODO: Make these be absolute path
file(GLOB ALL_GCOV_FILES ${COV_PATH}/*.gcov)

# Get only the filenames to use for filtering.
set(COVERAGE_SRCS_NAMES "")
foreach (COVSRC ${COVERAGE_SRCS})
	get_filename_component(COVSRC_NAME ${COVSRC} NAME)
	message("${COVSRC} -> ${COVSRC_NAME}")
	list(APPEND COVERAGE_SRCS_NAMES "${COVSRC_NAME}")
endforeach()

# Filter out all but the gcov files we want.
set(GCOV_FILES "")
message("Look in coverage sources: ${COVERAGE_SRCS_NAMES}")
foreach (GCOV_FILE ${ALL_GCOV_FILES})

	# Get rid of .gcov and path info.
	get_filename_component(GCOV_FILE ${GCOV_FILE} NAME)
	string(REGEX REPLACE "\\.gcov$" "" GCOV_FILE_WEXT ${GCOV_FILE})
	#message("gcov file wext: ${GCOV_FILE_WEXT}")

	# Is this in the list of source files?
	# TODO: We want to match against relative path filenames from the source file root...
	list(FIND COVERAGE_SRCS_NAMES ${GCOV_FILE_WEXT} WAS_FOUND)

	if (NOT WAS_FOUND EQUAL -1)
		message("Found ${GCOV_FILE}")
		list(APPEND GCOV_FILES ${GCOV_FILE})
	else()
		message("Not found: ${GCOV_FILE}")
	endif()
endforeach()

message("Gcov files we want: ${GCOV_FILES}")

# TODO: Enable setting these
set(JSON_SERVICE_NAME "travis-ci")
set(JSON_SERVICE_JOB_ID $ENV{TRAVIS_JOB_ID})

set(JSON_TEMPLATE
"{
  \"service_name\": \"\@JSON_SERVICE_NAME\@\",
  \"service_job_id\": \"\@JSON_SERVICE_JOB_ID\@\",
  \"source_files\": \@JSON_GCOV_FILES\@
}"
)

set(SRC_FILE_TEMPLATE
"{
  \"name\": \"\@GCOV_FILE_WEXT\@\",
  \"source\": \"\@GCOV_FILE_SOURCE\@\",
  \"coverage\": \@GCOV_FILE_COVERAGE\@
}"
)

set(JSON_GCOV_FILES "[")

# Read the GCOV files line by line and get the coverage data.
foreach (GCOV_FILE ${GCOV_FILES})

	get_filename_component(GCOV_FILE ${GCOV_FILE} NAME)
	string(REGEX REPLACE "\\.gcov$" "" GCOV_FILE_WEXT ${GCOV_FILE})

	# Loads the gcov file as a list of lines.
	file(STRINGS ${GCOV_FILE} GCOV_LINES)

	# We want an array of coverage data and the
	# source as a single string with line endings as \n
	# start building them from the contents of the .gcov
	set(GCOV_FILE_COVERAGE "[")
	set(GCOV_FILE_SOURCE "")

	foreach (GCOV_LINE ${GCOV_LINES})

		# TODO: we might want to split this up into 3 separate regexps instead.
		# C code uses ; all over the place, but CMake uses that as a list delimiter as well
		# this means that "RES" in the below code won't contain 3 list items at all times
		# for each ; in a source code line, that counts as another element in the CMAke list.
		# So simply doing list(GET RES 2 SOURCE) won't get the entire source line.

		# Example of what we're parsing:
		# Hitcount  |Line | Source
		# "        8:   26:        if (!allowed || (strlen(allowed) == 0))"
		string(REGEX REPLACE 
			"^([^:]*):([^:]*):(.*)$" 
			"\\1;\\2;\\3"
			RES
			"${GCOV_LINE}")
		#message("Line: ${GCOV_LINE}")

		list(LENGTH RES RES_COUNT)
		if (RES_COUNT GREATER 2)
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
				# TODO: Look for LCOV_EXCL_LINE to get rid of false positives.

				# Replace line ending literals (\n) in printf strings and such with
				# escaped versions (\\n). Coveralls uses \n for line endings.
				# Also escape " chars so we don't produce invalid JSON.
				#string(REPLACE "\"" "\\\\\"" SOURCE "${SOURCE}")
				string(REPLACE "\\" "\\\\" SOURCE "${SOURCE}")
				string(REGEX REPLACE "\"" "\\\\\"" SOURCE "${SOURCE}")
				string(REPLACE "\t" "\\\\t" SOURCE "${SOURCE}")
				string(REPLACE "\r" "\\\\r" SOURCE "${SOURCE}")

				# According to http://json.org/ these should be escaped as well.
				# Don't know how to do that in CMake however...
				#string(REPLACE "\b" "\\\\b" SOURCE "${SOURCE}")
				#string(REPLACE "\f" "\\\\f" SOURCE "${SOURCE}")
				#string(REGEX REPLACE "\u([a-fA-F0-9]{4})" "\\\\u\\1" SOURCE "${SOURCE}")

				# Concatenate all the sources into one long string that can
				# be put in the JSON file.
				# (Below is CMake 3.0.0 only. I guess it's faster....)
				#string(CONCAT GCOV_FILE_SOURCE "${SOURCE}\\n") 
				set(GCOV_FILE_SOURCE "${GCOV_FILE_SOURCE}${SOURCE}\\n")
			endif()
		else()
			message(WARNING "Failed to properly parse line --> ${GCOV_LINE}")
		endif()
	endforeach()

	# Advanced way of removing the trailing comma in the JSON array.
	# "[1, 2, 3, " -> "[1, 2, 3"
	string(REGEX REPLACE ",[ ]*$" "" GCOV_FILE_COVERAGE ${GCOV_FILE_COVERAGE})

	# Append the trailing ] to complete the JSON array.
	set(GCOV_FILE_COVERAGE "${GCOV_FILE_COVERAGE}]")

	# Generate the final JSON for this file.
	string(CONFIGURE ${SRC_FILE_TEMPLATE} FILE_JSON)
	#message("${FILE_JSON}")

	set(JSON_GCOV_FILES "${JSON_GCOV_FILES}${FILE_JSON}, ")
endforeach()

# Get rid of trailing comma.
string(REGEX REPLACE ",[ ]*$" "" JSON_GCOV_FILES ${JSON_GCOV_FILES})
set(JSON_GCOV_FILES "${JSON_GCOV_FILES}]")

# TODO: Also include files specified in COVERAGE_SRCS WITHOUT coverage data!

# Generate the final complete JSON!
string(CONFIGURE ${JSON_TEMPLATE} JSON)

#message("${JSON}")
file(WRITE "${COVERALLS_OUTPUT_FILE}" "${JSON}")
message("###########################################################################")
message("Generated coveralls JSON containing coverage data: ${COVERALLS_OUTPUT_FILE}")
message("###########################################################################")

