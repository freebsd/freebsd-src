if(UNIX)
  check_function_exists(crypt HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
  if(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
    set(HAVE_CRYPT TRUE)
  else(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
    check_library_exists(crypt crypt "" HAVE_CRYPT_IN_LIBCRYPT)
    if(HAVE_CRYPT_IN_LIBCRYPT)
      set(RPCAPD_LINK_LIBRARIES ${RPCAPD_LINK_LIBRARIES} crypt)
      set(HAVE_CRYPT TRUE)
    else(HAVE_CRYPT_IN_LIBCRYPT)
      message(WARNING "crypt() not found. Won't be able to build rpcapd.")
    endif(HAVE_CRYPT_IN_LIBCRYPT)
  endif(HAVE_CRYPT_IN_SYSTEM_LIBRARIES)
endif(UNIX)

#
# On UN*X, we need pthreads and crypt().
#
if(WIN32 OR ((CMAKE_USE_PTHREADS_INIT OR PTHREADS_FOUND) AND HAVE_CRYPT))
  if(UNIX)
    #
    # Do we have getspnam()?
    #
    check_function_exists(getspnam HAVE_GETSPNAM)

    #
    # Find library needed for getaddrinfo.
    # NOTE: if you hand check_library_exists as its last argument a variable
    # that's been set, it skips the test, so we need different variables.
    #
    include(CheckLibraryExists)
    check_function_exists(getaddrinfo STDLIBS_HAVE_GETADDRINFO)
    if(NOT STDLIBS_HAVE_GETADDRINFO)
      check_library_exists(xnet getaddrinfo "" LIBXNET_HAS_GETADDRINFO)
      if(LIBXNET_HAS_GETADDRINFO)
        set(RPCAPD_LINK_LIBRARIES ${RPCAPD_LINK_LIBRARIES} xnet)
      else(LIBXNET_HAS_GETADDRINFO)
        include(CMakePushCheckState)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_LIBRARIES nsl)
        check_library_exists(socket getaddrinfo "" LIBSOCKET_HAS_GETADDRINFO)
        cmake_pop_check_state()
        if(LIBSOCKET_HAS_GETADDRINFO)
          set(RPCAPD_LINK_LIBRARIES ${RPCAPD_LINK_LIBRARIES} socket nsl)
        endif(LIBSOCKET_HAS_GETADDRINFO)
      endif(LIBXNET_HAS_GETADDRINFO)
    endif(NOT STDLIBS_HAVE_GETADDRINFO)
  endif(UNIX)

  if(WIN32)
    set(RPCAPD_EXTRA_SOURCES
        win32-svc.c
        ${pcap_SOURCE_DIR}/missing/getopt.c
        ${pcap_SOURCE_DIR}/missing/win_snprintf.c
        rpcapd.rc)
    include_directories(${pcap_SOURCE_DIR}/rpcapd ${pcap_SOURCE_DIR}/missing)
  endif(WIN32)

  add_executable(rpcapd
    daemon.c
    fileconf.c
    log.c
    rpcapd.c
    ${pcap_SOURCE_DIR}/rpcap-protocol.c
    ${pcap_SOURCE_DIR}/sockutils.c
    ${pcap_SOURCE_DIR}/fmtutils.c
    ${RPCAPD_EXTRA_SOURCES}
  )

  if(NOT C_ADDITIONAL_FLAGS STREQUAL "")
    set_target_properties(rpcapd PROPERTIES COMPILE_FLAGS ${C_ADDITIONAL_FLAGS})
  endif()

  #
  # By default, build rpcapd universal with the appropriate set of
  # architectures for the OS on which we're doing the build.
  #
  if(APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    #
    # Get the major version of Darwin.
    #
    string(REGEX MATCH "^([0-9]+)" SYSTEM_VERSION_MAJOR "${CMAKE_SYSTEM_VERSION}")

    if(SYSTEM_VERSION_MAJOR EQUAL 9)
      #
      # Leopard.  Build for 32-bit x86 and 32-bit PowerPC, with
      # 32-bit x86 first.
      #
      set(OSX_PROGRAM_ARCHITECTURES "i386;ppc")
    elseif(SYSTEM_VERSION_MAJOR EQUAL 10)
      #
      # Snow Leopard.  Build for x86-64 and 32-bit x86, with
      # x86-64 first.
      #
      set(OSX_PROGRAM_ARCHITECTURES "x86_64;i386")
    else()
      #
      # Post-Snow Leopard.  Build only for x86-64.
      # XXX - update if and when Apple adds ARM-based Macs.
      # (You're on your own for iOS etc.)
      #
      set(OSX_PROGRAM_ARCHITECTURES "x86_64")
    endif()

    set_target_properties(rpcapd PROPERTIES
      OSX_ARCHITECTURES "${OSX_PROGRAM_ARCHITECTURES}")
  endif()

  if(WIN32)
    target_link_libraries(rpcapd ${LIBRARY_NAME}
      ${RPCAPD_LINK_LIBRARIES} ${PCAP_LINK_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
  else(WIN32)
    target_link_libraries(rpcapd ${LIBRARY_NAME}_static
      ${RPCAPD_LINK_LIBRARIES} ${PCAP_LINK_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
  endif(WIN32)

  ######################################
  # Install rpcap daemon and man pages
  ######################################

  #
  # "Define GNU standard installation directories", which actually
  # are also defined, to some degree, by autotools, and at least
  # some of which are general UN*X conventions.
  #
  include(GNUInstallDirs)

  set(MANADMIN_EXPAND rpcapd.manadmin.in)

  set(MANFILE_EXPAND rpcapd-config.manfile.in)

  if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    install(TARGETS rpcapd DESTINATION bin/amd64)
  else(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    install(TARGETS rpcapd DESTINATION bin)
  endif(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)

  # On UN*X, and on Windows when not using MSVC, generate process man
  # pages and arrange that they be installed.
  if(NOT MSVC)
    #
    # Man pages.
    #
    # For each section of the manual for which we have man pages
    # that require macro expansion, do the expansion.
    #
    set(MANADMIN "")
    foreach(TEMPLATE_MANPAGE ${MANADMIN_EXPAND})
        string(REPLACE ".manadmin.in" ".${MAN_ADMIN_COMMANDS}" MANPAGE ${TEMPLATE_MANPAGE})
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${TEMPLATE_MANPAGE} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE} @ONLY)
        set(MANADMIN ${MANADMIN} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE})
    endforeach(TEMPLATE_MANPAGE)
    install(FILES ${MANADMIN} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${MAN_ADMIN_COMMANDS})

    set(MANFILE "")
    foreach(TEMPLATE_MANPAGE ${MANFILE_EXPAND})
        string(REPLACE ".manfile.in" ".${MAN_FILE_FORMATS}" MANPAGE ${TEMPLATE_MANPAGE})
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${TEMPLATE_MANPAGE} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE} @ONLY)
        set(MANFILE ${MANFILE} ${CMAKE_CURRENT_BINARY_DIR}/${MANPAGE})
    endforeach(TEMPLATE_MANPAGE)
    install(FILES ${MANFILE} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${MAN_FILE_FORMATS})
  endif(NOT MSVC)
endif(WIN32 OR ((CMAKE_USE_PTHREADS_INIT OR PTHREADS_FOUND) AND HAVE_CRYPT))
