############################################
#
# How to build bsdtar_test
#
############################################
IF(ENABLE_CAT AND ENABLE_TEST)
  SET(bsdcat_test_SOURCES
    ../../test_utils/test_utils.c
    main.c
    test.h
    test_0.c
    test_empty_gz.c
    test_empty_lz4.c
    test_empty_xz.c
    test_error.c
    test_error_mixed.c
    test_expand_Z.c
    test_expand_bz2.c
    test_expand_gz.c
    test_expand_lz4.c
    test_expand_mixed.c
    test_expand_plain.c
    test_expand_xz.c
    test_help.c
    test_version.c
  )

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdcat_test ${bsdcat_test_SOURCES})
  SET_PROPERTY(TARGET bsdcat_test PROPERTY COMPILE_DEFINITIONS LIST_H)

  #
  # Generate list.h by grepping DEFINE_TEST() lines out of the C sources.
  #
  GENERATE_LIST_H(${CMAKE_CURRENT_BINARY_DIR}/list.h
    ${CMAKE_CURRENT_LIST_FILE} ${bsdcat_test_SOURCES})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR})

  # list.h has a line DEFINE_TEST(testname) for every
  # test.  We can use that to define the tests for cmake by
  # defining a DEFINE_TEST macro and reading list.h in.
  MACRO (DEFINE_TEST _testname)
    ADD_TEST(
      NAME bsdcat_${_testname}
      COMMAND bsdcat_test -vv
                          -p $<TARGET_FILE:bsdcat>
                          -r ${CMAKE_CURRENT_SOURCE_DIR}
                          ${_testname})
  ENDMACRO (DEFINE_TEST _testname)

  INCLUDE(${CMAKE_CURRENT_BINARY_DIR}/list.h)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/test_utils)

  # Experimental new test handling
  ADD_CUSTOM_TARGET(run_bsdcat_test
	COMMAND	bsdcat_test -p $<TARGET_FILE:bsdcat>
			    -r ${CMAKE_CURRENT_SOURCE_DIR})
  ADD_DEPENDENCIES(run_bsdcat_test bsdcat)
  ADD_DEPENDENCIES(run_all_tests run_bsdcat_test)

ENDIF(ENABLE_CAT AND ENABLE_TEST)
