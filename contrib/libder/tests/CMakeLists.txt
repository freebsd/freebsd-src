set(FUZZERS fuzz_parallel fuzz_stream fuzz_write)
set(UTILS )
set(TESTS test_privkey test_pubkey)

set(ALL_TESTS ${UTILS} ${TESTS})

if(BUILD_FUZZERS)
	set(UTILS ${UTILS} make_corpus)
	set(ALL_TESTS ${ALL_TESTS} ${FUZZERS} make_corpus)

	foreach(fuzzer IN LISTS FUZZERS)
		add_executable(${fuzzer} ${fuzzer}.c)

		target_compile_options(${fuzzer} PUBLIC -fsanitize=fuzzer)
		target_link_options(${fuzzer} PUBLIC -fsanitize=fuzzer)
	endforeach()

	target_link_options(fuzz_parallel PUBLIC -pthread)
endif()

foreach(prog IN LISTS UTILS TESTS)
	add_executable(${prog} ${prog}.c)
endforeach()

foreach(prog IN LISTS ALL_TESTS)
	target_include_directories(${prog} PRIVATE ${CMAKE_SOURCE_DIR}/libder)
	target_link_libraries(${prog} der_static)
endforeach()

add_custom_command(TARGET test_privkey POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_CURRENT_SOURCE_DIR}/repo.priv ${CMAKE_CURRENT_BINARY_DIR}/repo.priv)
add_custom_command(TARGET test_pubkey POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${CMAKE_CURRENT_SOURCE_DIR}/repo.pub ${CMAKE_CURRENT_BINARY_DIR}/repo.pub)

add_custom_target(check
	DEPENDS test_pubkey test_privkey
	COMMAND "${CMAKE_CURRENT_BINARY_DIR}/test_pubkey"
	COMMAND "${CMAKE_CURRENT_BINARY_DIR}/test_privkey"
)
