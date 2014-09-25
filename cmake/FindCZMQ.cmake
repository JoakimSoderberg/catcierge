# - Try to find CZMQ
# Once done this will define
#
#  CZMQ_FOUND - system has CZMQ
#  CZMQ_INCLUDE_DIRS - the CZMQ include directory
#  CZMQ_LIBRARIES - Link these to use CZMQ
#  CZMQ_DEFINITIONS - Compiler switches required for using CZMQ
#
#  Copyright (c) 2011 Lee Hambley <lee.hambley@gmail.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (CZMQ_LIBRARIES AND CZMQ_INCLUDE_DIRS)
  # in cache already
  set(CZMQ_FOUND TRUE)
else (CZMQ_LIBRARIES AND CZMQ_INCLUDE_DIRS)

  find_path(CZMQ_INCLUDE_DIR
    NAMES
      czmq.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(CZMQ_LIBRARY
    NAMES
      czmq
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  set(CZMQ_INCLUDE_DIRS
    ${CZMQ_INCLUDE_DIR}
  )

  if (CZMQ_LIBRARY)
    set(CZMQ_LIBRARIES
        ${CZMQ_LIBRARIES}
        ${CZMQ_LIBRARY}
    )
  endif (CZMQ_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(CZMQ DEFAULT_MSG CZMQ_LIBRARIES CZMQ_INCLUDE_DIRS)

  # show the CZMQ_INCLUDE_DIRS and CZMQ_LIBRARIES variables only in the advanced view
  mark_as_advanced(CZMQ_INCLUDE_DIRS CZMQ_LIBRARIES)

endif (CZMQ_LIBRARIES AND CZMQ_INCLUDE_DIRS)



