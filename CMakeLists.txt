# Copyright (c) 2013-2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#  * Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 2.8.6)

project(PT C)

# versioning
#
# the major and the minor number define the supported Intel PT set.
#
# a build number and a version extension can be optionally specified.
#
set(PT_VERSION_MAJOR 1)
set(PT_VERSION_MINOR 6)
set(PT_VERSION_BUILD "0" CACHE STRING "")
set(PT_VERSION_EXT "" CACHE STRING "")

set(PT_VERSION "${PT_VERSION_MAJOR}.${PT_VERSION_MINOR}.${PT_VERSION_BUILD}")

add_definitions(
  -DPT_VERSION_MAJOR=${PT_VERSION_MAJOR}
  -DPT_VERSION_MINOR=${PT_VERSION_MINOR}
  -DPT_VERSION_BUILD=${PT_VERSION_BUILD}
  -DPT_VERSION_EXT=\"${PT_VERSION_EXT}\"
)

include(GNUInstallDirs)
include(FindUnixCommands)
include(CheckCCompilerFlag)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(MAN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/man)

set(CMAKE_COLOR_MAKEFILE   OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_MACOSX_RPATH ON)

option(FEATURE_THREADS "A small amount of multi-threading support." ON)
if (FEATURE_THREADS)
  add_definitions(-DFEATURE_THREADS)
endif (FEATURE_THREADS)

option(DEVBUILD "Enable compiler warnings and turn them into errors." OFF)

option(PTDUMP "Enable ptdump, a packet dumper")
option(PTXED  "Enable ptxed, an instruction flow dumper")
option(PTTC   "Enable pttc, a test compiler")
option(PTUNIT "Enable ptunit, a unit test system and libipt unit tests")
option(MAN "Enable man pages (requires pandoc)." OFF)
option(SIDEBAND "Enable libipt-sb, a sideband correlation library")

if (SIDEBAND)
  option(PEVENT "Enable perf_event sideband support." OFF)
endif (SIDEBAND)

if (PTXED OR PEVENT)
  option(FEATURE_ELF "Support ELF files." OFF)
endif (PTXED OR PEVENT)

set(PTT OFF)
if (BASH AND PTDUMP AND PTXED AND PTTC)
  set(PTT ON)
endif ()

if (PTUNIT OR PTT)
  ENABLE_TESTING()
endif()

if (PTUNIT)
  enable_language(CXX)
endif()

include_directories(
  include
  ${CMAKE_CURRENT_BINARY_DIR}/libipt/include
)

if (PTUNIT)
  include_directories(
    ptunit/include
  )
endif (PTUNIT)

if (FEATURE_ELF)
  add_definitions(
    -DFEATURE_ELF
  )
endif (FEATURE_ELF)

if (SIDEBAND)
  add_definitions(
    -DFEATURE_SIDEBAND
  )

  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/sideband/include
  )
endif (SIDEBAND)

if (PEVENT)
  add_definitions(
    -DFEATURE_PEVENT
  )

  include_directories(
    pevent/include
  )
endif (PEVENT)


function(add_cflag_if_available option)

  check_c_compiler_flag(${option} ${option}_supported)
  if (${option}_supported)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${option}" PARENT_SCOPE)
  endif (${option}_supported)

endfunction(add_cflag_if_available)


if (CMAKE_HOST_WIN32)
  include_directories(
    include/windows
  )

  add_definitions(
    # cl spells inline __inline in C
    #
    /Dinline=__inline

    # cl spells strtoll _strtoi64
    #
    /Dstrtoll=_strtoi64

    # cl spells strtoull _strtoui64
    #
    /Dstrtoull=_strtoui64

    # avoid annoying warnings about unsecure standard functions
    #
    /D_CRT_SECURE_NO_WARNINGS
  )

  # enable parallel build
  #
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

  if (DEVBUILD)
    # compiler warnings
    #
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")

    # warnings are errors
    #
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
  endif (DEVBUILD)

  if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
    # prevent complaints on:
    # - do {} while(0) constructs
    # - int arr[] constructs
    #
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4127")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4200")

  endif (CMAKE_C_COMPILER_ID MATCHES "MSVC")

endif (CMAKE_HOST_WIN32)

if (CMAKE_HOST_UNIX)
  include_directories(
    include/posix
  )

  add_definitions(
    -D_POSIX_C_SOURCE=200809L
  )

  option(GCOV "Compile for GNU code coverage analysis." OFF)

  if (GCOV)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage")

    link_libraries(gcov)
  endif (GCOV)

  if (FEATURE_THREADS)
    link_libraries(pthread)
  endif (FEATURE_THREADS)

  # set the language
  #
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

  # windows-like dll export model
  #
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

  if (DEVBUILD)
    # compiler warnings
    #
    if (CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Weverything")

      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-disabled-macro-expansion")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-covered-switch-default")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-sign-conversion")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch-enum")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-cast-align")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-padded")
    else (CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")

      add_cflag_if_available("-Wimplicit-fallthrough=5")
    endif (CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")

    # warnings are errors
    #
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
  endif (DEVBUILD)

endif (CMAKE_HOST_UNIX)


function(add_ptunit_test_base name)
  if (PTUNIT)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ptunit)

    add_test(NAME ${name} COMMAND ${name})
  endif (PTUNIT)
endfunction(add_ptunit_test_base)

function(add_ptunit_c_test name)
    add_ptunit_test_base(ptunit-${name} test/src/ptunit-${name}.c ${ARGN})
endfunction(add_ptunit_c_test)

function(add_ptunit_cpp_test name)
    add_ptunit_test_base(ptunit-${name} test/src/ptunit-${name}.cpp ${ARGN})
endfunction(add_ptunit_cpp_test)

function(add_ptunit_libraries name)
  if (PTUNIT)
    target_link_libraries(ptunit-${name} ${ARGN})
  endif (PTUNIT)
endfunction(add_ptunit_libraries)


add_subdirectory(libipt)

if (PTDUMP)
  add_subdirectory(ptdump)
endif (PTDUMP)
if (PTXED)
  add_subdirectory(ptxed)
endif (PTXED)
if (PTTC)
  add_subdirectory(pttc)
endif (PTTC)
if (PTUNIT)
  add_subdirectory(ptunit)
endif (PTUNIT)
if (PTT)
  add_subdirectory(test)
endif (PTT)
if (MAN)
  add_subdirectory(doc/man)
endif (MAN)
if (SIDEBAND)
  add_subdirectory(sideband)
endif (SIDEBAND)
if (PEVENT)
  add_subdirectory(pevent)
endif (PEVENT)
