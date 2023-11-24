add_executable(fuzz_pcap onefile.c fuzz_pcap.c)
target_link_libraries(fuzz_pcap ${ARGN} ${LIBRARY_NAME}_static ${PCAP_LINK_LIBRARIES})
if(NOT "${SANITIZER_FLAGS}" STREQUAL "")
  set_target_properties(fuzz_pcap PROPERTIES
      LINK_FLAGS "${SANITIZER_FLAGS}")
endif()

add_executable(fuzz_filter onefile.c fuzz_filter.c)
target_link_libraries(fuzz_filter ${ARGN} ${LIBRARY_NAME}_static ${PCAP_LINK_LIBRARIES})
if(NOT "${SANITIZER_FLAGS}" STREQUAL "")
  set_target_properties(fuzz_filter PROPERTIES
      LINK_FLAGS "${SANITIZER_FLAGS}")
endif()

add_executable(fuzz_both onefile.c fuzz_both.c)
target_link_libraries(fuzz_both ${ARGN} ${LIBRARY_NAME}_static ${PCAP_LINK_LIBRARIES})
if(NOT "${SANITIZER_FLAGS}" STREQUAL "")
  set_target_properties(fuzz_both PROPERTIES
      LINK_FLAGS "${SANITIZER_FLAGS}")
endif()

if(ENABLE_REMOTE AND "$ENV{CFLAGS}" MATCHES "-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION")
add_executable(fuzz_rclient onefile.c fuzz_rclient.c)
target_link_libraries(fuzz_rclient ${ARGN} ${LIBRARY_NAME}_static ${PCAP_LINK_LIBRARIES})
if(NOT "${SANITIZER_FLAGS}" STREQUAL "")
  set_target_properties(fuzz_rclient PROPERTIES
      LINK_FLAGS "${SANITIZER_FLAGS}")
endif()

add_executable(fuzz_rserver onefile.c fuzz_rserver.c ../../rpcapd/daemon.c)
check_function_exists(crypt HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
if(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
    set(HAVE_CRYPT TRUE)
else(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
     set(PCAP_LINK_LIBRARIES ${PCAP_LINK_LIBRARIES} crypt)
endif(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
target_link_libraries(fuzz_rserver ${ARGN} ${LIBRARY_NAME}_static ${PCAP_LINK_LIBRARIES})

if(NOT "${SANITIZER_FLAGS}" STREQUAL "")
  set_target_properties(fuzz_rserver PROPERTIES
      LINK_FLAGS "${SANITIZER_FLAGS}")
endif()
endif(ENABLE_REMOTE AND "$ENV{CFLAGS}" MATCHES "-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION")
