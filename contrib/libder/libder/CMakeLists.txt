file(GLOB libder_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(der SHARED ${libder_SOURCES})
add_library(der_static STATIC ${libder_SOURCES})

if(BUILD_FUZZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(der PUBLIC -fsanitize=fuzzer-no-link)
	target_link_options(der PUBLIC -fsanitize=fuzzer-no-link)

	target_compile_options(der_static PUBLIC -fsanitize=fuzzer-no-link)
	target_link_options(der_static PUBLIC -fsanitize=fuzzer-no-link)
endif()
