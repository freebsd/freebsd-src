############################################
#
# How to build bsdunzip_test
#
############################################
IF(ENABLE_UNZIP AND ENABLE_TEST)
  SET(bsdunzip_test_SOURCES
    ../../libarchive_fe/lafe_err.c
    ../../test_utils/test_utils.c
    ../../test_utils/test_main.c
    test.h
    test_0.c
    test_basic.c
    test_doubledash.c
    test_glob.c
    test_not_exist.c
    test_singlefile.c
    test_C.c
    test_p.c
    test_d.c
    test_j.c
    test_L.c
    test_n.c
    test_o.c
    test_q.c
    test_t.c
    test_t_bad.c
    test_version.c
    test_x.c
    test_Z1.c
    test_P_encryption.c
    test_I.c
  )

  #
  # Register target
  #
  ADD_EXECUTABLE(bsdunzip_test ${bsdunzip_test_SOURCES})
  IF(ENABLE_ACL)
    SET(TEST_ACL_LIBS "")
    IF(HAVE_LIBACL)
      LIST(APPEND TEST_ACL_LIBS ${ACL_LIBRARY})
    ENDIF(HAVE_LIBACL)
    IF(HAVE_LIBRICHACL)
      LIST(APPEND TEST_ACL_LIBS ${RICHACL_LIBRARY})
    ENDIF(HAVE_LIBRICHACL)
    TARGET_LINK_LIBRARIES(bsdunzip_test ${TEST_ACL_LIBS})
  ENDIF(ENABLE_ACL)
  SET_PROPERTY(TARGET bsdunzip_test PROPERTY COMPILE_DEFINITIONS LIST_H)

  #
  # Generate list.h by grepping DEFINE_TEST() lines out of the C sources.
  #
  GENERATE_LIST_H(${CMAKE_CURRENT_BINARY_DIR}/list.h
    ${CMAKE_CURRENT_LIST_FILE} bsdunzip_test ${bsdunzip_test_SOURCES})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR})

  DISCOVER_TESTS(bsdunzip ${bsdunzip_test_SOURCES})

  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/test_utils)
  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/unzip/test)

  # Experimental new test handling
  ADD_CUSTOM_TARGET(run_bsdunzip_test
	COMMAND	bsdunzip_test -p $<TARGET_FILE:bsdunzip>
			     -r ${CMAKE_CURRENT_SOURCE_DIR}
			     -vv)
  ADD_DEPENDENCIES(run_bsdunzip_test bsdunzip)
  ADD_DEPENDENCIES(run_all_tests run_bsdunzip_test)
ENDIF(ENABLE_UNZIP AND ENABLE_TEST)

