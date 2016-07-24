### cmake file for building libdivsufsort Package ###
cmake_minimum_required(VERSION 2.4.4)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
include(AppendCompilerFlags)

## Project information ##
project(libdivsufsort C)
set(PROJECT_VENDOR "Yuta Mori")
set(PROJECT_CONTACT "yuta.256@gmail.com")
set(PROJECT_URL "https://github.com/y-256/libdivsufsort")
set(PROJECT_DESCRIPTION "A lightweight suffix sorting library")
include(VERSION.cmake)

## CPack configuration ##
set(CPACK_GENERATOR "TGZ;TBZ2;ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2;ZIP")
include(ProjectCPack)

## Project options ##
option(BUILD_SHARED_LIBS "Set to OFF to build static libraries" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_DIVSUFSORT64 "Build libdivsufsort64" OFF)
option(USE_OPENMP "Use OpenMP for parallelization" OFF)
option(WITH_LFS "Enable Large File Support" ON)

## Installation directories ##
set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32 or 64)")

set(CMAKE_INSTALL_RUNTIMEDIR "" CACHE PATH "Specify the output directory for dll runtimes (default is bin)")
if(NOT CMAKE_INSTALL_RUNTIMEDIR)
  set(CMAKE_INSTALL_RUNTIMEDIR "${CMAKE_INSTALL_PREFIX}/bin")
endif(NOT CMAKE_INSTALL_RUNTIMEDIR)

set(CMAKE_INSTALL_LIBDIR "" CACHE PATH "Specify the output directory for libraries (default is lib)")
if(NOT CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
endif(NOT CMAKE_INSTALL_LIBDIR)

set(CMAKE_INSTALL_INCLUDEDIR "" CACHE PATH "Specify the output directory for header files (default is include)")
if(NOT CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
endif(NOT CMAKE_INSTALL_INCLUDEDIR)

set(CMAKE_INSTALL_PKGCONFIGDIR "" CACHE PATH "Specify the output directory for pkgconfig files (default is lib/pkgconfig)")
if(NOT CMAKE_INSTALL_PKGCONFIGDIR)
  set(CMAKE_INSTALL_PKGCONFIGDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif(NOT CMAKE_INSTALL_PKGCONFIGDIR)

## Build type ##
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(NOT CMAKE_BUILD_TYPE)

## Compiler options ##
if(MSVC)
  append_c_compiler_flags("/W4" "VC" CMAKE_C_FLAGS)
  append_c_compiler_flags("/Oi;/Ot;/Ox;/Oy" "VC" CMAKE_C_FLAGS_RELEASE)
  if(USE_OPENMP)
    append_c_compiler_flags("/openmp" "VC" CMAKE_C_FLAGS)
  endif(USE_OPENMP)
elseif(BORLAND)
  append_c_compiler_flags("-w" "BCC" CMAKE_C_FLAGS)
  append_c_compiler_flags("-Oi;-Og;-Os;-Ov;-Ox" "BCC" CMAKE_C_FLAGS_RELEASE)
else(MSVC)
  if(CMAKE_COMPILER_IS_GNUCC)
    append_c_compiler_flags("-Wall" "GCC" CMAKE_C_FLAGS)
    append_c_compiler_flags("-fomit-frame-pointer" "GCC" CMAKE_C_FLAGS_RELEASE)
    if(USE_OPENMP)
      append_c_compiler_flags("-fopenmp" "GCC" CMAKE_C_FLAGS)
    endif(USE_OPENMP)
  else(CMAKE_COMPILER_IS_GNUCC)
    append_c_compiler_flags("-Wall" "UNKNOWN" CMAKE_C_FLAGS)
    append_c_compiler_flags("-fomit-frame-pointer" "UNKNOWN" CMAKE_C_FLAGS_RELEASE)
    if(USE_OPENMP)
      append_c_compiler_flags("-fopenmp;-openmp;-omp" "UNKNOWN" CMAKE_C_FLAGS)
    endif(USE_OPENMP)
  endif(CMAKE_COMPILER_IS_GNUCC)
endif(MSVC)

## Add definitions ##
add_definitions(-DHAVE_CONFIG_H=1 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)

## Add subdirectories ##
add_subdirectory(pkgconfig)
add_subdirectory(include)
add_subdirectory(lib)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif(BUILD_EXAMPLES)

## Add 'uninstall' target ##
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeModules/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/CMakeModules/cmake_uninstall.cmake")
