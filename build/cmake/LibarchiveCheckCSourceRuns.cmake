# - Check if the given C source code compiles and runs.
# CHECK_C_SOURCE_RUNS(<code> <var>)
#  <code>   - source code to try to compile
#  <var>    - variable to store the result
#             (1 for success, empty for failure)
# The following variables may be set before calling this macro to
# modify the way the check is run:
#
#  CMAKE_REQUIRED_FLAGS = string of compile command line flags
#  CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#  CMAKE_REQUIRED_INCLUDES = list of include directories
#  CMAKE_REQUIRED_LIBRARIES = list of libraries to link

#=============================================================================
# Copyright 2006-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

#
# Extra arguments added by libarchive
# CMAKE_REQUIRED_LINKER_FLAGS = string of linker command line flags
#

include(CMakeExpandImportedTargets)


macro(LIBARCHIVE_CHECK_C_SOURCE_RUNS SOURCE VAR)
  if("${VAR}" MATCHES "^${VAR}$")
    set(MACRO_CHECK_FUNCTION_DEFINITIONS
      "-D${VAR} ${CMAKE_REQUIRED_FLAGS}")
    if(CMAKE_REQUIRED_LIBRARIES)
      # this one translates potentially used imported library targets to their files on disk
      CMAKE_EXPAND_IMPORTED_TARGETS(_ADJUSTED_CMAKE_REQUIRED_LIBRARIES  LIBRARIES  ${CMAKE_REQUIRED_LIBRARIES} CONFIGURATION "${CMAKE_TRY_COMPILE_CONFIGURATION}")
      set(CHECK_C_SOURCE_COMPILES_ADD_LIBRARIES
        "-DLINK_LIBRARIES:STRING=${_ADJUSTED_CMAKE_REQUIRED_LIBRARIES}")
    else()
      set(CHECK_C_SOURCE_COMPILES_ADD_LIBRARIES)
    endif()
    if(CMAKE_REQUIRED_INCLUDES)
      set(CHECK_C_SOURCE_COMPILES_ADD_INCLUDES
        "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CHECK_C_SOURCE_COMPILES_ADD_INCLUDES)
    endif()
	if(CMAKE_REQUIRED_LINKER_FLAGS)
	  set(CHECK_C_SOURCE_COMPILES_ADD_LINKER_FLAGS
	    "-DCMAKE_EXE_LINKER_FLAGS:STRING=${CMAKE_REQUIRED_LINKER_FLAGS} -DCMAKE_SHARED_LINKER_FLAGS:STRING=${CMAKE_REQUIRED_LINKER_FLAGS} -DCMAKE_MODULE_LINKER_FLAGS:STRING=${CMAKE_REQUIRED_LINKER_FLAGS}")
	else()
	  set(CHECK_C_SOURCE_COMPILES_ADD_LINKER_FLAGS)
	endif()
    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c"
      "${SOURCE}\n")

    message(STATUS "Performing Test ${VAR}")
    try_run(${VAR}_EXITCODE ${VAR}_COMPILED
      ${CMAKE_BINARY_DIR}
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.c
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      CMAKE_FLAGS -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_FUNCTION_DEFINITIONS} ${CHECK_C_SOURCE_COMPILES_ADD_LINKER_FLAGS}
      -DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}
      "${CHECK_C_SOURCE_COMPILES_ADD_LIBRARIES}"
      "${CHECK_C_SOURCE_COMPILES_ADD_INCLUDES}"
      COMPILE_OUTPUT_VARIABLE OUTPUT)
    # if it did not compile make the return value fail code of 1
    if(NOT ${VAR}_COMPILED)
      set(${VAR}_EXITCODE 1)
    endif()
    # if the return value was 0 then it worked
    if("${${VAR}_EXITCODE}" EQUAL 0)
      set(${VAR} 1 CACHE INTERNAL "Test ${VAR}")
      message(STATUS "Performing Test ${VAR} - Success")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Performing C SOURCE FILE Test ${VAR} succeded with the following output:\n"
        "${OUTPUT}\n"
        "Return value: ${${VAR}}\n"
        "Source file was:\n${SOURCE}\n")
    else()
      if(CMAKE_CROSSCOMPILING AND "${${VAR}_EXITCODE}" MATCHES  "FAILED_TO_RUN")
        set(${VAR} "${${VAR}_EXITCODE}")
      else()
        set(${VAR} "" CACHE INTERNAL "Test ${VAR}")
      endif()

      message(STATUS "Performing Test ${VAR} - Failed")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Performing C SOURCE FILE Test ${VAR} failed with the following output:\n"
        "${OUTPUT}\n"
        "Return value: ${${VAR}_EXITCODE}\n"
        "Source file was:\n${SOURCE}\n")

    endif()
  endif()
endmacro()

