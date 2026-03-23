file(GLOB TESTS "*_test.c")

find_package(CMocka REQUIRED)

message(STATUS "CMocka vars: ${CMOCKA_LIBRARIES} ${CMOCKA_INCLUDE_DIR}")

find_library(MATH_LIBRARY m)

check_include_file("execinfo.h" HAS_EXECINFO)

foreach(test_file ${TESTS})
  string(REGEX REPLACE ".*/([^/]+).c" "\\1" NAME ${test_file})
  message("Adding test ${NAME}")
  add_executable(${NAME} "${NAME}.c" assertions.c stream_expectations.c
                         test_allocator.c)
  target_link_libraries(${NAME} ${CMOCKA_LIBRARIES})
  target_link_libraries(${NAME} cbor)
  if(MATH_LIBRARY)
    target_link_libraries(${NAME} ${MATH_LIBRARY})
  endif()
  target_include_directories(${NAME} PUBLIC ${CMOCKA_INCLUDE_DIR})
  add_test(NAME ${NAME} COMMAND ${NAME})
  add_dependencies(coverage ${NAME})
endforeach()

add_executable(cpp_linkage_test cpp_linkage_test.cpp)
target_link_libraries(cpp_linkage_test cbor)
