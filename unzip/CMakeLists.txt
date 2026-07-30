#
# SPDX-License-Identifier: BSD-2-Clause
#
############################################
#
# How to build bsdunzip
#
############################################
IF(ENABLE_UNZIP)

  SET(bsdunzip_SOURCES
    bsdunzip.c
    bsdunzip.h
    bsdunzip_platform.h
    cmdline.c
    la_queue.h
    ../libarchive_fe/lafe_err.c
    ../libarchive_fe/lafe_err.h
    ../libarchive_fe/lafe_fnmatch.c
    ../libarchive_fe/lafe_fnmatch.h
    ../libarchive_fe/lafe_getline.c
    ../libarchive_fe/lafe_getline.h
    ../libarchive_fe/lafe_platform.h
    ../libarchive_fe/passphrase.c
    ../libarchive_fe/passphrase.h
  )
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../libarchive_fe)
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdunzip_SOURCES bsdunzip_windows.c)
    LIST(APPEND bsdunzip_SOURCES bsdunzip_windows.h)
    IF(NOT MSVC)
      # The GNU toolchains (MinGW/Clang) do not generate an application
      # manifest, so we embed one through a resource script.
      ENABLE_LANGUAGE(RC)
      INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
      LIST(APPEND bsdunzip_SOURCES bsdunzip.rc)
      SET_SOURCE_FILES_PROPERTIES(bsdunzip.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bsdunzip.exe.manifest)
    ENDIF(NOT MSVC)
  ENDIF(WIN32 AND NOT CYGWIN)

  # bsdunzip documentation
  SET(bsdunzip_MANS bsdunzip.1)

  # How to build bsdunzip
  ADD_EXECUTABLE(bsdunzip ${bsdunzip_SOURCES})
  IF(ENABLE_UNZIP_SHARED)
    TARGET_LINK_LIBRARIES(bsdunzip archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_UNZIP_SHARED)
    TARGET_LINK_LIBRARIES(bsdunzip archive_static ${ADDITIONAL_LIBS})
    SET_TARGET_PROPERTIES(bsdunzip PROPERTIES COMPILE_DEFINITIONS
                                 LIBARCHIVE_STATIC)
  ENDIF(ENABLE_UNZIP_SHARED)
  IF(WIN32 AND NOT CYGWIN AND MSVC)
    # MSVC merges .manifest sources into the linker's default manifest
    TARGET_SOURCES(bsdunzip PRIVATE bsdunzip.exe.manifest)
  ENDIF(WIN32 AND NOT CYGWIN AND MSVC)

  # Installation rules
  IF(ENABLE_INSTALL)
    INSTALL(TARGETS bsdunzip RUNTIME DESTINATION bin)
    INSTALL_MAN(${bsdunzip_MANS})
  ENDIF(ENABLE_INSTALL)
ENDIF(ENABLE_UNZIP)

add_subdirectory(test)
