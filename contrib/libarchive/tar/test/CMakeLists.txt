############################################
#
# How to build bsdtar_test
#
############################################
IF(ENABLE_TAR AND ENABLE_TEST)
  SET(bsdtar_test_SOURCES
    ../getdate.c
    main.c
    test.h
    test_0.c
    test_basic.c
    test_copy.c
    test_empty_mtree.c
    test_getdate.c
    test_help.c
    test_option_T_upper.c
    test_option_q.c
    test_option_r.c
    test_option_s.c
    test_patterns.c
    test_stdio.c
    test_strip_components.c
    test_symlink_dir.c
    test_version.c
    test_windows.c
  )
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdtar_test_SOURCES ../bsdtar_windows.c)
    LIST(APPEND bsdtar_test_SOURCES ../bsdtar_windows.h)
  ENDIF(WIN32 AND NOT CYGWIN)

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdtar_test ${bsdtar_test_SOURCES})
  SET_PROPERTY(TARGET bsdtar_test PROPERTY COMPILE_DEFINITIONS LIST_H)

  #
  # Generate list.h by grepping DEFINE_TEST() lines out of the C sources.
  #
  GENERATE_LIST_H(${CMAKE_CURRENT_BINARY_DIR}/list.h
    ${CMAKE_CURRENT_LIST_FILE} ${bsdtar_test_SOURCES})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR})

  # list.h has a line DEFINE_TEST(testname) for every
  # test.  We can use that to define the tests for cmake by
  # defining a DEFINE_TEST macro and reading list.h in.
  MACRO (DEFINE_TEST _testname)
    ADD_TEST_28(
      NAME bsdtar_${_testname}
      COMMAND bsdtar_test -vv
                          -p $<TARGET_FILE:bsdtar>
                          -r ${CMAKE_CURRENT_SOURCE_DIR}
                          ${_testname})
  ENDMACRO (DEFINE_TEST _testname)

  INCLUDE(${CMAKE_CURRENT_BINARY_DIR}/list.h)

  # Experimental new test handling
  ADD_CUSTOM_TARGET(run_bsdtar_test
	COMMAND	bsdtar_test -p ${BSDTAR} -r ${CMAKE_CURRENT_SOURCE_DIR})
  ADD_DEPENDENCIES(run_bsdtar_test bsdtar)
  ADD_DEPENDENCIES(run_all_tests run_bsdtar_test)

ENDIF (ENABLE_TAR AND ENABLE_TEST)
