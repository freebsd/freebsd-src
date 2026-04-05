set(COMMON_TEST_INCLUDES
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/uthash
)

set(COMMON_TEST_LIBS ucl)

set(TEST_ENV_VARS
	"TEST_DIR=${CMAKE_SOURCE_DIR}/tests"
	"TEST_OUT_DIR=${CMAKE_BINARY_DIR}/tests"
	"TEST_BINARY_DIR=${CMAKE_BINARY_DIR}/tests"
)

macro(add_ucl_test testname sourcefile wrapper)
	add_executable(${testname} ${sourcefile})
	target_include_directories(${testname} PRIVATE ${COMMON_TEST_INCLUDES})
	target_link_libraries(${testname} PRIVATE ${COMMON_TEST_LIBS})
	IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_test(NAME ${testname} COMMAND ${CMAKE_SOURCE_DIR}/tests/${wrapper})
		set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${TEST_ENV_VARS}")
	ENDIF()
endmacro()

# Build test binaries always (not just for testing)
add_ucl_test(test_basic test_basic.c basic.test)
add_ucl_test(test_speed test_speed.c speed.test)
add_ucl_test(test_schema test_schema.c schema.test)
add_ucl_test(test_msgpack test_msgpack.c msgpack.test)
add_ucl_test(test_generate test_generate.c generate.test)

