# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Read README.cmake before using this.

PROJECT(APR-Util C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

FIND_PACKAGE(OpenSSL)

FIND_PACKAGE(expat)

OPTION(APU_HAVE_CRYPTO      "Crypto support"                            OFF)
OPTION(APU_HAVE_ODBC        "Build ODBC DBD driver"                     ON)
OPTION(APR_HAS_LDAP         "LDAP support"                              ON)
OPTION(INSTALL_PDB          "Install .pdb files (if generated)"         ON)
OPTION(APR_BUILD_TESTAPR    "Build the test suite"                      OFF)
OPTION(TEST_STATIC_LIBS     "Test programs use APR static libraries instead of shared libraries?" OFF)
SET(APR_INCLUDE_DIR         "${CMAKE_INSTALL_PREFIX}/include"           CACHE STRING "Directory with APR include files")
SET(APR_LIBRARIES           "${CMAKE_INSTALL_PREFIX}/lib/libapr-1.lib"  CACHE STRING "APR library to link with")

IF(NOT EXISTS "${APR_INCLUDE_DIR}/apr.h")
  MESSAGE(FATAL_ERROR "APR include directory ${APR_INCLUDE_DIR} is not correct.")
ENDIF()
FOREACH(onelib ${APR_LIBRARIES})
  IF(NOT EXISTS ${onelib})
    MESSAGE(FATAL_ERROR "APR library ${onelib} was not found.")
  ENDIF()
ENDFOREACH()

IF(APU_HAVE_CRYPTO)
IF(NOT OPENSSL_FOUND)
  MESSAGE(FATAL_ERROR "OpenSSL is the only supported crypto implementation, and it wasn't found!")
ENDIF()
ENDIF()

# create 1-or-0 representation of feature tests for apu.h

SET(apu_have_crypto_10 0)
SET(apu_have_apr_iconv_10 0) # not yet implemented
SET(apr_has_ldap_10 0)

IF(APU_HAVE_CRYPTO)
  SET(apu_have_crypto_10 1)
ENDIF()

IF(APR_HAS_LDAP)
  SET(apr_has_ldap_10 1)
ENDIF()

IF(NOT EXPAT_FOUND)
  MESSAGE(FATAL_ERROR "Expat is required, and it wasn't found!")
ENDIF()

SET(XMLLIB_INCLUDE_DIR   ${EXPAT_INCLUDE_DIRS})
SET(XMLLIB_LIBRARIES     ${EXPAT_LIBRARIES})

SET(LDAP_LIBRARIES)
IF(APR_HAS_LDAP)
  SET(LDAP_LIBRARIES wldap32)
ENDIF()

CONFIGURE_FILE(include/apu.hwc
              ${PROJECT_BINARY_DIR}/apu.h)
CONFIGURE_FILE(include/apr_ldap.hwc
              ${PROJECT_BINARY_DIR}/apr_ldap.h)
# "COPYONLY" just because anything else isn't implemented ;)
CONFIGURE_FILE(include/private/apu_config.hw
               ${PROJECT_BINARY_DIR}/apu_config.h
               COPYONLY)
CONFIGURE_FILE(include/private/apu_select_dbm.hw
               ${PROJECT_BINARY_DIR}/apu_select_dbm.h
               COPYONLY)
CONFIGURE_FILE(include/apu_want.hw
               ${PROJECT_BINARY_DIR}/apu_want.h
               COPYONLY)

# Generated .h files are stored in PROJECT_BINARY_DIR, not the
# source tree.
#
# BROKEN: not searching PROJECT_BINARY_DIR first, so you have to
#         manually delete apu.h in PROJECT_SOURCE_DIR/include if
#         you've generated apu.h before using a different build

SET(APR_INCLUDE_DIRECTORIES
  ${PROJECT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/private
  ${APR_INCLUDE_DIR}
)

INCLUDE_DIRECTORIES(${APR_INCLUDE_DIRECTORIES} ${XMLLIB_INCLUDE_DIR})

SET(APR_PUBLIC_HEADERS_STATIC
  include/apr_anylock.h
  include/apr_base64.h
  include/apr_buckets.h
  include/apr_crypto.h
  include/apr_date.h
  include/apr_dbd.h
  include/apr_dbm.h
  include/apr_hooks.h
  include/apr_ldap_init.h
  include/apr_ldap_option.h
  include/apr_ldap_rebind.h
  include/apr_ldap_url.h
  include/apr_md4.h
  include/apr_md5.h
  include/apr_memcache.h
  include/apr_optional.h
  include/apr_optional_hooks.h
  include/apr_queue.h
  include/apr_redis.h
  include/apr_reslist.h
  include/apr_rmm.h
  include/apr_sdbm.h
  include/apr_sha1.h
  include/apr_siphash.h
  include/apr_strmatch.h
  include/apr_thread_pool.h
  include/apr_uri.h
  include/apr_uuid.h
  include/apr_xlate.h
  include/apr_xml.h
  include/apu_errno.h
  include/apu_version.h
)

# apu_config.h and apu_select_dbm.h are private
SET(APR_PUBLIC_HEADERS_GENERATED
  ${PROJECT_BINARY_DIR}/apu.h
  ${PROJECT_BINARY_DIR}/apr_ldap.h
  ${PROJECT_BINARY_DIR}/apu_want.h
)

SET(APR_SOURCES
  buckets/apr_brigade.c
  buckets/apr_buckets.c
  buckets/apr_buckets_alloc.c
  buckets/apr_buckets_eos.c
  buckets/apr_buckets_file.c
  buckets/apr_buckets_flush.c
  buckets/apr_buckets_heap.c
  buckets/apr_buckets_mmap.c
  buckets/apr_buckets_pipe.c
  buckets/apr_buckets_pool.c
  buckets/apr_buckets_refcount.c
  buckets/apr_buckets_simple.c
  buckets/apr_buckets_socket.c
  crypto/apr_crypto.c
  crypto/apr_md4.c
  crypto/apr_md5.c
  crypto/apr_passwd.c
  crypto/apr_sha1.c
  crypto/apr_siphash.c
  crypto/crypt_blowfish.c
  crypto/getuuid.c
  crypto/uuid.c
  dbd/apr_dbd.c
  dbm/apr_dbm.c
  dbm/apr_dbm_sdbm.c
  dbm/sdbm/sdbm.c
  dbm/sdbm/sdbm_hash.c
  dbm/sdbm/sdbm_lock.c
  dbm/sdbm/sdbm_pair.c
  encoding/apr_base64.c
  hooks/apr_hooks.c
  memcache/apr_memcache.c
  misc/apr_date.c
  misc/apr_queue.c
  misc/apr_reslist.c
  misc/apr_rmm.c
  misc/apr_thread_pool.c
  misc/apu_dso.c
  misc/apu_version.c
  redis/apr_redis.c
  strmatch/apr_strmatch.c
  uri/apr_uri.c
  xlate/xlate.c
  xml/apr_xml.c
)

IF(APR_HAS_LDAP)
  SET(APR_SOURCES ${APR_SOURCES} ldap/apr_ldap_stub.c ldap/apr_ldap_url.c)
ENDIF()

SET(APR_TEST_SOURCES
  test/abts.c
  test/testbuckets.c
  test/testcrypto.c
  test/testdate.c
  test/testdbd.c
  test/testdbm.c
  test/testldap.c
  test/testmd4.c
  test/testmd5.c
  test/testmemcache.c
  test/testpass.c
  test/testqueue.c
  test/testredis.c
  test/testreslist.c
  test/testrmm.c
  test/testsiphash.c
  test/teststrmatch.c
  test/testuri.c
  test/testutil.c
  test/testuuid.c
  test/testxlate.c
  test/testxml.c
)

SET(install_targets)
SET(install_bin_pdb)
SET(dbd_drivers)

# Note: The WINNT definition on some targets is used only by libaprutil.rc.

# libaprutil-1 is shared, aprutil-1 is static
ADD_LIBRARY(libaprutil-1 SHARED ${APR_SOURCES} ${APR_PUBLIC_HEADERS_GENERATED} libaprutil.rc)
SET(install_targets ${install_targets} libaprutil-1)
SET(install_bin_pdb ${install_bin_pdb} ${PROJECT_BINARY_DIR}/libaprutil-1.pdb)
TARGET_LINK_LIBRARIES(libaprutil-1 ${APR_LIBRARIES} ${XMLLIB_LIBRARIES})
SET_TARGET_PROPERTIES(libaprutil-1 PROPERTIES COMPILE_DEFINITIONS "APU_DECLARE_EXPORT;APR_DECLARE_EXPORT;XML_STATIC;WINNT")

ADD_LIBRARY(aprutil-1 STATIC ${APR_SOURCES} ${APR_PUBLIC_HEADERS_GENERATED})
SET(install_targets ${install_targets} aprutil-1)
TARGET_LINK_LIBRARIES(aprutil-1 ${APR_LIBRARIES} ${XMLLIB_LIBRARIES})
SET_TARGET_PROPERTIES(aprutil-1 PROPERTIES COMPILE_DEFINITIONS "APU_DECLARE_STATIC;APR_DECLARE_STATIC;APU_DSO_MODULE_BUILD;XML_STATIC")

IF(APU_HAVE_CRYPTO)
  IF(NOT OPENSSL_FOUND)
    MESSAGE(FATAL_ERROR "Only OpenSSL-based crypto is currently implemented in the cmake build")
  ENDIF()
  ADD_LIBRARY(apr_crypto_openssl-1 SHARED crypto/apr_crypto_openssl.c libaprutil.rc)
  SET(install_targets ${install_targets} apr_crypto_openssl-1)
  SET(install_bin_pdb ${install_bin_pdb} ${PROJECT_BINARY_DIR}/apr_crypto_openssl-1.pdb)
  SET_TARGET_PROPERTIES(apr_crypto_openssl-1 PROPERTIES INCLUDE_DIRECTORIES "${APR_INCLUDE_DIRECTORIES};${OPENSSL_INCLUDE_DIR}")
  SET_TARGET_PROPERTIES(apr_crypto_openssl-1 PROPERTIES COMPILE_DEFINITIONS "WINNT")
  SET_TARGET_PROPERTIES(apr_crypto_openssl-1 PROPERTIES COMPILE_FLAGS "-DAPR_DECLARE_EXPORT=1 -DAPU_DECLARE_EXPORT=1 -DDLL_NAME=apr_crypto_openssl")
  TARGET_LINK_LIBRARIES(apr_crypto_openssl-1 libaprutil-1 ${APR_LIBRARIES} ${OPENSSL_LIBRARIES})
ENDIF()

IF(APU_HAVE_ODBC)
  ADD_LIBRARY(apr_dbd_odbc-1 SHARED dbd/apr_dbd_odbc.c libaprutil.rc)
  SET(install_targets ${install_targets} apr_dbd_odbc-1)
  SET(install_bin_pdb ${install_bin_pdb} ${PROJECT_BINARY_DIR}/apr_dbd_odbc-1.pdb)
  SET(dbd_drivers ${dbd_drivers} odbc)
  TARGET_LINK_LIBRARIES(apr_dbd_odbc-1 libaprutil-1 ${APR_LIBRARIES} odbc32 odbccp32)
  SET_PROPERTY(TARGET apr_dbd_odbc-1 APPEND PROPERTY LINK_FLAGS /export:apr_dbd_odbc_driver)
  SET_TARGET_PROPERTIES(apr_dbd_odbc-1 PROPERTIES COMPILE_DEFINITIONS "APU_HAVE_ODBC;HAVE_SQL_H;APU_DECLARE_EXPORT;APR_DECLARE_EXPORT;APU_DSO_MODULE_BUILD;WINNT")
  SET_TARGET_PROPERTIES(apr_dbd_odbc-1 PROPERTIES COMPILE_FLAGS "-DAPR_DECLARE_EXPORT=1 -DAPU_DECLARE_EXPORT=1 -DDLL_NAME=apr_dbd_odbc")
ENDIF()

IF(APR_HAS_LDAP)
  ADD_LIBRARY(apr_ldap-1 SHARED ldap/apr_ldap_init.c ldap/apr_ldap_option.c 
              ldap/apr_ldap_rebind.c libaprutil.rc)
  SET(install_targets ${install_targets} apr_ldap-1)
  SET(install_bin_pdb ${install_bin_pdb} ${PROJECT_BINARY_DIR}/apr_ldap-1.pdb)
  TARGET_LINK_LIBRARIES(apr_ldap-1 libaprutil-1 ${APR_LIBRARIES} ${LDAP_LIBRARIES})
  SET_TARGET_PROPERTIES(apr_ldap-1 PROPERTIES COMPILE_DEFINITIONS "WINNT")
  SET_TARGET_PROPERTIES(apr_ldap-1 PROPERTIES COMPILE_FLAGS "-DAPR_DECLARE_EXPORT=1 -DAPU_DECLARE_EXPORT=1 -DDLL_NAME=apr_ldap")
  SET(apr_ldap_libraries apr_ldap-1)
ELSE()
  SET(apr_ldap_libraries)
ENDIF()

IF(APR_BUILD_TESTAPR)
  ENABLE_TESTING()
  # Create a "check" target that displays test program output to the console.
  ADD_CUSTOM_TARGET(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

  # copy data files to build directory so that we can run programs from there
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory 
                  ${PROJECT_BINARY_DIR}/data)
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                  ${PROJECT_SOURCE_DIR}/test/data/billion-laughs.xml
                  ${PROJECT_BINARY_DIR}/data/billion-laughs.xml)

  IF(TEST_STATIC_LIBS)
    SET(whichapr    aprutil-1)
    SET(apiflag     "-DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC")
  ELSE()
    SET(whichapr    libaprutil-1)
    SET(apiflag)
  ENDIF()

  ADD_EXECUTABLE(testall ${APR_TEST_SOURCES})
  TARGET_LINK_LIBRARIES(testall ${whichapr} ${apr_ldap_libraries} ${XMLLIB_LIBRARIES} ${LDAP_LIBRARIES})
  IF(apiflag)
    SET_TARGET_PROPERTIES(testall PROPERTIES COMPILE_FLAGS ${apiflag})
  ENDIF()
  ADD_TEST(NAME testall COMMAND testall)

  ADD_EXECUTABLE(dbd test/dbd.c)
  TARGET_LINK_LIBRARIES(dbd ${whichapr})
  IF(apiflag)
    SET_TARGET_PROPERTIES(dbd PROPERTIES COMPILE_FLAGS ${apiflag})
  ENDIF()

  # dbd is run multiple times with different parameters.
  FOREACH(somedbd ${dbd_drivers})
    ADD_TEST(NAME dbd-${somedbd} COMMAND dbd ${somedbd})
  ENDFOREACH()

ENDIF (APR_BUILD_TESTAPR)

# Installation

INSTALL(TARGETS ${install_targets}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
       )

IF(INSTALL_PDB)
  INSTALL(FILES ${install_bin_pdb}
          DESTINATION bin
          CONFIGURATIONS RelWithDebInfo Debug)
ENDIF()

INSTALL(FILES ${APR_PUBLIC_HEADERS_STATIC} ${APR_PUBLIC_HEADERS_GENERATED} DESTINATION include)

STRING(TOUPPER "${CMAKE_BUILD_TYPE}" buildtype)
MESSAGE(STATUS "")
MESSAGE(STATUS "")
MESSAGE(STATUS "APR-Util configuration summary:")
MESSAGE(STATUS "")
MESSAGE(STATUS "  Build type ...................... : ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "  Install .pdb (if available)...... : ${INSTALL_PDB}")
MESSAGE(STATUS "  Install prefix .................. : ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "  C compiler ...................... : ${CMAKE_C_COMPILER}")
MESSAGE(STATUS "  APR include directory ........... : ${APR_INCLUDE_DIR}")
MESSAGE(STATUS "  APR libraries ................... : ${APR_LIBRARIES}")
MESSAGE(STATUS "  DBD ODBC driver ................. : ${APU_HAVE_ODBC}")
MESSAGE(STATUS "  APU_HAVE_CRYPTO ................. : ${APU_HAVE_CRYPTO}")
MESSAGE(STATUS "  APR_HAS_LDAP .................... : ${APR_HAS_LDAP}")
MESSAGE(STATUS "  Build test suite ................ : ${APR_BUILD_TESTAPR}")
IF(TEST_STATIC_LIBS)
MESSAGE(STATUS "    (testing static libraries)")
ELSE()
MESSAGE(STATUS "    (testing dynamic libraries)")
ENDIF()
