cmake_minimum_required(VERSION 3.18)

project(libder)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
		add_compile_options(-fsanitize=address,undefined -fstrict-aliasing)
		add_link_options(-fsanitize=address,undefined -fstrict-aliasing)
	endif()

	add_compile_options(-Werror)
endif()

# AppleClang is excluded for the time being; the version used in GitHub Action
# runners doesn't seem to have that part of libclang_rt installed, though the
# -fsanitize=fuzzer-no-link instrumentation seems to be fine.  Maybe re-evaluate
# this for MATCHES as a possibility later.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	set(BUILD_FUZZERS TRUE
		CACHE BOOL "Build the libFuzzer fuzzers (needs llvm)")
else()
	set(BUILD_FUZZERS FALSE
		CACHE BOOL "Build the libFuzzer fuzzers (needs llvm)")
endif()

add_subdirectory(libder)
add_subdirectory(derdump)
add_subdirectory(tests)
