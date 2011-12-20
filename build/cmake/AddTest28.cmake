# - Macro approximating the CMake 2.8 ADD_TEST(NAME) signature.
# ADD_TEST_28(NAME <name> COMMAND <command> [arg1 [arg2 ...]])
#  <name>    - The name of the test
#  <command> - The test executable
#  [argN...] - Arguments to the test executable
# This macro approximates the ADD_TEST(NAME) signature provided in
# CMake 2.8 but works with CMake 2.6 too.  See CMake 2.8 documentation
# of ADD_TEST()for details.
#
# This macro automatically replaces a <command> that names an
# executable target with the target location.  A generator expression
# of the form "$<TARGET_FILE:tgt>" is supported in both the command
# and arguments of the test.  Howerver, this macro only works for
# targets without per-config output name properties set.
#
# Example usage:
#   add_test(NAME mytest COMMAND testDriver --exe $<TARGET_FILE:myexe>)
# This creates a test "mytest" whose command runs a testDriver tool
# passing the full path to the executable file produced by target
# "myexe".

#=============================================================================
# Copyright 2009 Kitware, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

CMAKE_MINIMUM_REQUIRED(VERSION 2.6.3)

# CMake 2.8 supports ADD_TEST(NAME) natively.
IF(NOT "${CMAKE_VERSION}" VERSION_LESS "2.8")
  MACRO(ADD_TEST_28)
    ADD_TEST(${ARGV})
  ENDMACRO()
  RETURN()
ENDIF()

# Simulate ADD_TEST(NAME) signature from CMake 2.8.
MACRO(ADD_TEST_28 NAME name COMMAND command)
  # Enforce the signature.
  IF(NOT "x${NAME}" STREQUAL "xNAME")
    MESSAGE(FATAL_ERROR "First ADD_TEST_28 argument must be \"NAME\"")
  ENDIF()
  IF(NOT "x${COMMAND}" STREQUAL "xCOMMAND")
    MESSAGE(FATAL_ERROR "Third ADD_TEST_28 argument must be \"COMMAND\"")
  ENDIF()

  # Perform "COMMAND myexe ..." substitution.
  SET(cmd "${command}")
  IF(TARGET "${cmd}")
    _ADD_TEST_28_GET_EXE(${cmd} cmd)
  ENDIF()

  # Perform "COMMAND ... $<TARGET_FILE:myexe> ..." substitution.
  SET(target_file "\\$<TARGET_FILE:(.+)>")
  SET(args)
  FOREACH(ARG ${cmd} ${ARGN})
    SET(arg "${ARG}")
    IF("${arg}" MATCHES "${target_file}")
      STRING(REGEX REPLACE "${target_file}" "\\1" tgt "${arg}")
      IF(TARGET "${tgt}")
        _ADD_TEST_28_GET_EXE(${tgt} exe)
        STRING(REGEX REPLACE "${target_file}" "${exe}" arg "${arg}")
      ENDIF()
    ENDIF()
    LIST(APPEND args "${arg}")
  ENDFOREACH()

  # Invoke old ADD_TEST() signature with transformed arguments.
  ADD_TEST(${name} ${args})
ENDMACRO()

# Get the test-time location of an executable target.
MACRO(_ADD_TEST_28_GET_EXE tgt exe_var)
  # The LOCATION property gives a build-time location.
  GET_TARGET_PROPERTY(${exe_var} ${tgt} LOCATION)

  # In single-configuration generatrs the build-time and test-time
  # locations are the same because there is no per-config variable
  # reference.  In multi-configuration generators the following
  # substitution converts the build-time configuration variable
  # reference to a test-time configuration variable reference.
  IF(CMAKE_CONFIGURATION_TYPES)
    STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CTEST_CONFIGURATION_TYPE}"
      ${exe_var} "${${exe_var}}")
  ENDIF(CMAKE_CONFIGURATION_TYPES)
ENDMACRO()
