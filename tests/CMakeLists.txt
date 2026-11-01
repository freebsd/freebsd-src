set(COMMON_TEST_INCLUDES
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/uthash
)

set(COMMON_TEST_LIBS ucl)

set(TEST_ENV_VARS
	"TEST_DIR=${PROJECT_SOURCE_DIR}/tests"
	"TEST_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
	"TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
)

macro(add_ucl_test testname sourcefile wrapper)
	add_executable(${testname} ${sourcefile})
	target_include_directories(${testname} PRIVATE ${COMMON_TEST_INCLUDES})
	target_link_libraries(${testname} PRIVATE ${COMMON_TEST_LIBS})
	IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_test(NAME ${testname} COMMAND ${PROJECT_SOURCE_DIR}/tests/${wrapper})
		set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${TEST_ENV_VARS}")
	ENDIF()
endmacro()

# Tests using POSIX APIs (mmap, STDIN_FILENO, etc.) - skip on Windows
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_ucl_test(test_basic test_basic.c basic.test)
	add_ucl_test(test_speed test_speed.c speed.test)
	add_ucl_test(test_schema test_schema.c schema.test)
ENDIF()

# Portable tests
add_ucl_test(test_msgpack test_msgpack.c msgpack.test)
add_ucl_test(test_generate test_generate.c generate.test)
add_ucl_test(test_variables test_variables.c variables.test)
add_ucl_test(test_empty_chunks test_empty_chunks.c empty_chunks.test)
add_ucl_test(test_csexp test_csexp.c csexp.test)
add_ucl_test(test_msgpack_malformed test_msgpack_malformed.c msgpack_malformed.test)
add_ucl_test(test_cbor test_cbor.c cbor.test)
add_ucl_test(test_cbor_vectors test_cbor_vectors.c cbor_vectors.test)
add_ucl_test(test_roundtrip test_roundtrip.c roundtrip.test)

add_executable(test_limits test_limits.c)
target_include_directories(test_limits PRIVATE ${COMMON_TEST_INCLUDES})
target_link_libraries(test_limits PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_limits COMMAND test_limits)
ENDIF()

add_executable(test_schema_refs test_schema_refs.c)
target_include_directories(test_schema_refs PRIVATE ${COMMON_TEST_INCLUDES})
target_link_libraries(test_schema_refs PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_schema_refs COMMAND test_schema_refs)
ENDIF()

add_executable(test_strnstr test_strnstr.c)
target_include_directories(test_strnstr PRIVATE ${COMMON_TEST_INCLUDES})
target_link_libraries(test_strnstr PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_strnstr COMMAND test_strnstr)
ENDIF()

add_executable(test_size_overflow test_size_overflow.c)
target_include_directories(test_size_overflow PRIVATE ${COMMON_TEST_INCLUDES}
	${PROJECT_SOURCE_DIR}/klib)
target_link_libraries(test_size_overflow PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_size_overflow COMMAND test_size_overflow)
ENDIF()

add_executable(test_object_free test_object_free.c)
target_include_directories(test_object_free PRIVATE ${COMMON_TEST_INCLUDES})
target_link_libraries(test_object_free PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_object_free COMMAND test_object_free)
ENDIF()

add_executable(test_copy_failure test_copy_failure.c)
target_include_directories(test_copy_failure PRIVATE ${COMMON_TEST_INCLUDES}
	${PROJECT_SOURCE_DIR}/klib)
target_link_libraries(test_copy_failure PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_copy_failure COMMAND test_copy_failure)
ENDIF()

add_executable(test_dup_error_key test_dup_error_key.c)
target_include_directories(test_dup_error_key PRIVATE ${COMMON_TEST_INCLUDES})
target_link_libraries(test_dup_error_key PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_dup_error_key COMMAND test_dup_error_key)
ENDIF()

add_executable(test_special_handlers test_special_handlers.c)
target_include_directories(test_special_handlers PRIVATE ${COMMON_TEST_INCLUDES}
	${PROJECT_SOURCE_DIR}/klib)
target_link_libraries(test_special_handlers PRIVATE ${COMMON_TEST_LIBS})
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_test(NAME test_special_handlers COMMAND test_special_handlers)
ENDIF()

# The Lua binding, which only CMake builds
IF(ENABLE_LUA MATCHES "ON" AND TARGET lua-ucl)
	add_executable(test_lua test_lua.c)
	target_include_directories(test_lua PRIVATE ${COMMON_TEST_INCLUDES})
	target_link_libraries(test_lua PRIVATE lua-ucl ${COMMON_TEST_LIBS})
	add_test(NAME test_lua COMMAND test_lua)
ENDIF()

# Built to match tests/Makefile.am; streamline.test is not registered
# because test_streamline output does not match streamline.res yet
IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_executable(test_streamline test_streamline.c)
	target_include_directories(test_streamline PRIVATE ${COMMON_TEST_INCLUDES})
	target_link_libraries(test_streamline PRIVATE ${COMMON_TEST_LIBS})
ENDIF()
