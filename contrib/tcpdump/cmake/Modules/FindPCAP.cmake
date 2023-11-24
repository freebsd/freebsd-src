#
# Try to find libpcap.
#
# To tell this module where to look, a user may set the environment variable
# PCAP_ROOT to point cmake to the *root* of a directory with include and
# lib subdirectories for pcap.dll (e.g WpdPack or npcap-sdk).
# Alternatively, PCAP_ROOT may also be set from cmake command line or GUI
# (e.g cmake -DPCAP_ROOT=C:\path\to\pcap [...])
#

if(WIN32)
  #
  # Building for Windows.
  #
  # libpcap isn't set up to install .pc files or pcap-config on Windows,
  # and it's not clear that either of them would work without a lot
  # of additional effort.  WinPcap doesn't supply them, and neither
  # does Npcap.
  #
  # So just search for them directly.  Look for both pcap and wpcap.
  # Don't bother looking for static libraries; unlike most UN*Xes
  # (with the exception of AIX), where different extensions are used
  # for shared and static, Windows uses .lib both for import libraries
  # for DLLs and for static libraries.
  #
  # We don't directly set PCAP_INCLUDE_DIRS or PCAP_LIBRARIES, as
  # they're not supposed to be cache entries, and find_path() and
  # find_library() set cache entries.
  #
  find_path(PCAP_INCLUDE_DIR pcap.h)

  # The 64-bit Packet.lib is located under /x64
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    #
    # For the WinPcap and Npcap SDKs, the Lib subdirectory of the top-level
    # directory contains 32-bit libraries; the 64-bit libraries are in the
    # Lib/x64 directory.
    #
    # The only way to *FORCE* CMake to look in the Lib/x64 directory
    # without searching in the Lib directory first appears to be to set
    # CMAKE_LIBRARY_ARCHITECTURE to "x64".
    #
    set(CMAKE_LIBRARY_ARCHITECTURE "x64")
  endif()
  find_library(PCAP_LIBRARY NAMES pcap wpcap)

  #
  # Do the standard arg processing, including failing if it's a
  # required package.
  #
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(PCAP
    DEFAULT_MSG
    PCAP_INCLUDE_DIR
    PCAP_LIBRARY
  )
  mark_as_advanced(
    PCAP_INCLUDE_DIR
    PCAP_LIBRARY
  )
  if(PCAP_FOUND)
    set(PCAP_LIBRARIES ${PCAP_LIBRARY})
    set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
  endif()
else(WIN32)
  #
  # Building for UN*X.
  #
  # See whether we were handed a QUIET argument, so we can pass it on
  # to pkg_search_module.  Do *NOT* pass on the REQUIRED argument,
  # because, if pkg-config isn't found, or it is but it has no .pc
  # files for libpcap, that is *not* necessarily an indication that
  # libpcap isn't available - not all systems ship pkg-config, and
  # libpcap didn't have .pc files until libpcap 1.9.0.
  #
  if(PCAP_FIND_QUIETLY)
    set(_quiet "QUIET")
  endif()

  #
  # First, try pkg-config.
  # Before doing so, set the PKG_CONFIG_PATH environment variable
  # to include all the directories in CMAKE_PREFIX_PATH.
  #
  # *If* we were to require CMake 3.1 or later on UN*X,
  # pkg_search_module() would do this for us, but, for now,
  # we're not doing that, in case somebody's building with
  # CMake on some "long-term support" version, predating
  # CMake 3.1, of an OS that supplies an earlier
  # version as a package.
  #
  # If we ever set a minimum of 3.1 or later on UN*X, we should
  # remove the environment variable changes.
  #
  # This is based on code in the CMake 3.12.4 FindPkgConfig.cmake,
  # which is "Distributed under the OSI-approved BSD 3-Clause License."
  #
  find_package(PkgConfig)

  #
  # Get the current PKG_CONFIG_PATH setting.
  #
  set(_pkg_config_path "$ENV{PKG_CONFIG_PATH}")

  #
  # Save it, so we can restore it after we run pkg-config.
  #
  set(_saved_pkg_config_path "${_pkg_config_path}")

  if(NOT "${CMAKE_PREFIX_PATH}" STREQUAL "")
    #
    # Convert it to a CMake-style path, before we add additional
    # values to it.
    #
    if(NOT "${_pkg_config_path}" STREQUAL "")
      file(TO_CMAKE_PATH "${_pkg_config_path}" _pkg_config_path)
    endif()

    #
    # Turn CMAKE_PREFIX_PATH into a list of extra paths to add
    # to _pkg_config_path.
    #
    set(_extra_paths "")
    list(APPEND _extra_paths ${CMAKE_PREFIX_PATH})

    # Create a list of the possible pkgconfig subfolder (depending on
    # the system
    set(_lib_dirs)
    if(NOT DEFINED CMAKE_SYSTEM_NAME
        OR (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$"
            AND NOT CMAKE_CROSSCOMPILING))
      if(EXISTS "/etc/debian_version") # is this a debian system ?
        if(CMAKE_LIBRARY_ARCHITECTURE)
          list(APPEND _lib_dirs "lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig")
        endif()
      else()
        # not debian, check the FIND_LIBRARY_USE_LIB32_PATHS and FIND_LIBRARY_USE_LIB64_PATHS properties
        get_property(uselib32 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS)
        if(uselib32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
          list(APPEND _lib_dirs "lib32/pkgconfig")
        endif()
        get_property(uselib64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
        if(uselib64 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
          list(APPEND _lib_dirs "lib64/pkgconfig")
        endif()
        get_property(uselibx32 GLOBAL PROPERTY FIND_LIBRARY_USE_LIBX32_PATHS)
        if(uselibx32 AND CMAKE_INTERNAL_PLATFORM_ABI STREQUAL "ELF X32")
          list(APPEND _lib_dirs "libx32/pkgconfig")
        endif()
      endif()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND NOT CMAKE_CROSSCOMPILING)
      list(APPEND _lib_dirs "libdata/pkgconfig")
    endif()
    list(APPEND _lib_dirs "lib/pkgconfig")
    list(APPEND _lib_dirs "share/pkgconfig")

    # Check if directories exist and eventually append them to the
    # pkgconfig path list
    foreach(_prefix_dir ${_extra_paths})
      foreach(_lib_dir ${_lib_dirs})
        if(EXISTS "${_prefix_dir}/${_lib_dir}")
          list(APPEND _pkg_config_path "${_prefix_dir}/${_lib_dir}")
          list(REMOVE_DUPLICATES _pkg_config_path)
        endif()
      endforeach()
    endforeach()

    if(NOT "${_pkg_config_path}" STREQUAL "")
      # remove empty values from the list
      list(REMOVE_ITEM _pkg_config_path "")
      file(TO_NATIVE_PATH "${_pkg_config_path}" _pkg_config_path)
      if(UNIX)
        string(REPLACE ";" ":" _pkg_config_path "${_pkg_config_path}")
        string(REPLACE "\\ " " " _pkg_config_path "${_pkg_config_path}")
      endif()
      set(ENV{PKG_CONFIG_PATH} "${_pkg_config_path}")
    endif()
  endif()
  pkg_search_module(CONFIG_PCAP ${_quiet} libpcap)
  set(ENV{PKG_CONFIG_PATH} "${_saved_pkg_config_path}")

  if(NOT CONFIG_PCAP_FOUND)
    #
    # That didn't work.  Try pcap-config.
    #
    find_program(PCAP_CONFIG pcap-config)
    if(PCAP_CONFIG)
      #
      # We have pcap-config; use it.
      #
      if(NOT "${_quiet}" STREQUAL "QUIET")
        message(STATUS "Found pcap-config")
      endif()

      #
      # If this is a vendor-supplied pcap-config, which we define as
      # being "a pcap-config in /usr/bin or /usr/ccs/bin" (the latter
      # is for Solaris and Sun/Oracle Studio), there are some issues.
      # Work around them.
      #
      if("${PCAP_CONFIG}" STREQUAL /usr/bin/pcap-config OR
         "${PCAP_CONFIG}" STREQUAL /usr/ccs/bin/pcap-config)
        #
        # It's vendor-supplied.
        #
        if(APPLE)
          #
          # This is macOS or another Darwin-based OS.
          #
          # That means that /usr/bin/pcap-config it may provide
          # -I/usr/local/include with --cflags and -L/usr/local/lib
          # with --libs; if there's no pcap installed under /usr/local,
          # that will cause the build to fail, and if there is a pcap
          # installed there, you'll get that pcap even if you don't
          # want it.  Remember that, so we ignore those values.
          #
          set(_broken_apple_pcap_config TRUE)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS" AND CMAKE_SYSTEM_VERSION MATCHES "5[.][0-9.]*")
          #
          # This is Solaris 2 or later, i.e. SunOS 5.x.
          #
          # At least on Solaris 11; there's /usr/bin/pcap-config, which
          # reports -L/usr/lib with --libs, causing the 32-bit libraries
          # to be found, and there's /usr/bin/{64bitarch}/pcap-config,
          # where {64bitarch} is a name for the 64-bit version of the
          # instruction set, which reports -L /usr/lib/{64bitarch},
          # causing the 64-bit libraries to be found.
          #
          # So if we're building 64-bit targets, we replace PCAP_CONFIG
          # with /usr/bin/{64bitarch}; we get {64bitarch} as the
          # output of "isainfo -n".
          #
          if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            execute_process(COMMAND "isainfo" "-n"
              RESULT_VARIABLE ISAINFO_RESULT
              OUTPUT_VARIABLE ISAINFO_OUTPUT
              OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(ISAINFO_RESULT EQUAL 0)
              #
              # Success - change PCAP_CONFIG.
              #
              string(REPLACE "/bin/" "/bin/${ISAINFO_OUTPUT}/" PCAP_CONFIG "${PCAP_CONFIG}")
            endif()
          endif()
        endif()
      endif()

      #
      # Now get the include directories.
      #
      execute_process(COMMAND "${PCAP_CONFIG}" "--cflags"
        RESULT_VARIABLE PCAP_CONFIG_RESULT
        OUTPUT_VARIABLE PCAP_CONFIG_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(NOT PCAP_CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "pcap-config --cflags failed")
      endif()
      separate_arguments(CFLAGS_LIST UNIX_COMMAND ${PCAP_CONFIG_OUTPUT})
      set(CONFIG_PCAP_INCLUDE_DIRS "")
      foreach(_arg IN LISTS CFLAGS_LIST)
        if(_arg MATCHES "^-I")
          #
          # Extract the directory by removing the -I.
          #
          string(REGEX REPLACE "-I" "" _dir ${_arg})
          #
          # Work around macOS (and probably other Darwin) brokenness,
          # by not adding /usr/local/include if it's from the broken
          # Apple pcap-config.
          #
          if(NOT _broken_apple_pcap_config OR
             NOT "${_dir}" STREQUAL /usr/local/include)
            # Add it to CONFIG_PCAP_INCLUDE_DIRS
            list(APPEND CONFIG_PCAP_INCLUDE_DIRS ${_dir})
          endif()
        endif()
      endforeach()

      #
      # Now, get the library directories and libraries for dynamic linking.
      #
      execute_process(COMMAND "${PCAP_CONFIG}" "--libs"
        RESULT_VARIABLE PCAP_CONFIG_RESULT
        OUTPUT_VARIABLE PCAP_CONFIG_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(NOT PCAP_CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "pcap-config --libs failed")
      endif()
      separate_arguments(LIBS_LIST UNIX_COMMAND ${PCAP_CONFIG_OUTPUT})
      set(CONFIG_PCAP_LIBRARY_DIRS "")
      set(CONFIG_PCAP_LIBRARIES "")
      foreach(_arg IN LISTS LIBS_LIST)
        if(_arg MATCHES "^-L")
          #
          # Extract the directory by removing the -L.
          #
          string(REGEX REPLACE "-L" "" _dir ${_arg})
          #
          # Work around macOS (and probably other Darwin) brokenness,
          # by not adding /usr/local/lib if it's from the broken
          # Apple pcap-config.
          #
          if(NOT _broken_apple_pcap_config OR
             NOT "${_dir}" STREQUAL /usr/local/lib)
            # Add this directory to CONFIG_PCAP_LIBRARY_DIRS
            list(APPEND CONFIG_PCAP_LIBRARY_DIRS ${_dir})
          endif()
        elseif(_arg MATCHES "^-l")
          string(REGEX REPLACE "-l" "" _lib ${_arg})
          list(APPEND CONFIG_PCAP_LIBRARIES ${_lib})
        endif()
      endforeach()

      #
      # Now, get the library directories and libraries for static linking.
      #
      execute_process(COMMAND "${PCAP_CONFIG}" "--libs" "--static"
        RESULT_VARIABLE PCAP_CONFIG_RESULT
        OUTPUT_VARIABLE PCAP_CONFIG_OUTPUT
      )
      if(NOT PCAP_CONFIG_RESULT EQUAL 0)
        message(FATAL_ERROR "pcap-config --libs --static failed")
      endif()
      separate_arguments(LIBS_LIST UNIX_COMMAND ${PCAP_CONFIG_OUTPUT})
      set(CONFIG_PCAP_STATIC_LIBRARY_DIRS "")
      set(CONFIG_PCAP_STATIC_LIBRARIES "")
      foreach(_arg IN LISTS LIBS_LIST)
        if(_arg MATCHES "^-L")
          #
          # Extract the directory by removing the -L.
          #
          string(REGEX REPLACE "-L" "" _dir ${_arg})
          #
          # Work around macOS (and probably other Darwin) brokenness,
          # by not adding /usr/local/lib if it's from the broken
          # Apple pcap-config.
          #
          if(NOT _broken_apple_pcap_config OR
             NOT "${_dir}" STREQUAL /usr/local/lib)
            # Add this directory to CONFIG_PCAP_STATIC_LIBRARY_DIRS
            list(APPEND CONFIG_PCAP_STATIC_LIBRARY_DIRS ${_dir})
          endif()
        elseif(_arg MATCHES "^-l")
          string(REGEX REPLACE "-l" "" _lib ${_arg})
          #
          # Try to find that library, so we get its full path, as
          # we do with dynamic libraries.
          #
          list(APPEND CONFIG_PCAP_STATIC_LIBRARIES ${_lib})
        endif()
      endforeach()

      #
      # We've set CONFIG_PCAP_INCLUDE_DIRS, CONFIG_PCAP_LIBRARIES, and
      # CONFIG_PCAP_STATIC_LIBRARIES above; set CONFIG_PCAP_FOUND.
      #
      set(CONFIG_PCAP_FOUND YES)
    endif()
  endif()

  #
  # If CONFIG_PCAP_FOUND is set, we have information from pkg-config and
  # pcap-config; we need to convert library names to library full paths.
  #
  # If it's not set, we have to look for the libpcap headers and library
  # ourselves.
  #
  if(CONFIG_PCAP_FOUND)
    #
    # Use CONFIG_PCAP_INCLUDE_DIRS as the value for PCAP_INCLUDE_DIRS.
    #
    set(PCAP_INCLUDE_DIRS "${CONFIG_PCAP_INCLUDE_DIRS}")

    #
    # CMake *really* doesn't like the notion of specifying
    # "here are the directories in which to look for libraries"
    # except in find_library() calls; it *really* prefers using
    # full paths to library files, rather than library names.
    #
    foreach(_lib IN LISTS CONFIG_PCAP_LIBRARIES)
      find_library(_libfullpath ${_lib} HINTS ${CONFIG_PCAP_LIBRARY_DIRS})
      list(APPEND PCAP_LIBRARIES ${_libfullpath})
      #
      # Remove that from the cache; we're using it as a local variable,
      # but find_library insists on making it a cache variable.
      #
      unset(_libfullpath CACHE)
   endforeach()

    #
    # Now do the same for the static libraries.
    #
    set(SAVED_CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES}")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    foreach(_lib IN LISTS CONFIG_PCAP_STATIC_LIBRARIES)
      find_library(_libfullpath ${_lib} HINTS ${CONFIG_PCAP_LIBRARY_DIRS})
      list(APPEND PCAP_STATIC_LIBRARIES ${_libfullpath})
      #
      # Remove that from the cache; we're using it as a local variable,
      # but find_library insists on making it a cache variable.
      #
      unset(_libfullpath CACHE)
    endforeach()
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${SAVED_CMAKE_FIND_LIBRARY_SUFFIXES}")

    #
    # We found libpcap using pkg-config or pcap-config.
    #
    set(PCAP_FOUND YES)
  else(CONFIG_PCAP_FOUND)
    #
    # We didn't have pkg-config, or we did but it didn't have .pc files
    # for libpcap, and we don't have pkg-config, so we have to look for
    # the headers and libraries ourself.
    #
    # We don't directly set PCAP_INCLUDE_DIRS or PCAP_LIBRARIES, as
    # they're not supposed to be cache entries, and find_path() and
    # find_library() set cache entries.
    #
    # Try to find the header file.
    #
    find_path(PCAP_INCLUDE_DIR pcap.h)

    #
    # Try to find the library
    #
    find_library(PCAP_LIBRARY pcap)

    # Try to find the static library (XXX - what about AIX?)
    set(SAVED_CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES}")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    find_library(PCAP_STATIC_LIBRARY pcap)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${SAVED_CMAKE_FIND_LIBRARY_SUFFIXES}")

    #
    # This will fail if REQUIRED is set and PCAP_INCLUDE_DIR or
    # PCAP_LIBRARY aren't set.
    #
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(PCAP
      DEFAULT_MSG
      PCAP_INCLUDE_DIR
      PCAP_LIBRARY
    )

    mark_as_advanced(
      PCAP_INCLUDE_DIR
      PCAP_LIBRARY
      PCAP_STATIC_LIBRARY
    )

    if(PCAP_FOUND)
      set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
      set(PCAP_LIBRARIES ${PCAP_LIBRARY})
      set(PCAP_STATIC_LIBRARIES ${PCAP_STATIC_LIBRARY})
    endif(PCAP_FOUND)
  endif(CONFIG_PCAP_FOUND)
endif(WIN32)
