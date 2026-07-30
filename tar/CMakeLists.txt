#
# SPDX-License-Identifier: BSD-2-Clause
#
############################################
#
# How to build bsdtar
#
############################################
IF(ENABLE_TAR)

  SET(bsdtar_SOURCES
    bsdtar.c
    bsdtar.h
    bsdtar_platform.h
    cmdline.c
    creation_set.c
    read.c
    subst.c
    util.c
    write.c
    ../libarchive_fe/lafe_err.c
    ../libarchive_fe/lafe_err.h
    ../libarchive_fe/lafe_platform.h
    ../libarchive_fe/line_reader.c
    ../libarchive_fe/line_reader.h
    ../libarchive_fe/passphrase.c
    ../libarchive_fe/passphrase.h
  )
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../libarchive_fe)
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdtar_SOURCES bsdtar_windows.c)
    LIST(APPEND bsdtar_SOURCES bsdtar_windows.h)
    IF(NOT MSVC)
      # The GNU toolchains (MinGW/Clang) do not generate an application
      # manifest, so we embed one through a resource script.
      ENABLE_LANGUAGE(RC)
      INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
      LIST(APPEND bsdtar_SOURCES bsdtar.rc)
      SET_SOURCE_FILES_PROPERTIES(bsdtar.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bsdtar.exe.manifest)
    ENDIF(NOT MSVC)
  ENDIF(WIN32 AND NOT CYGWIN)

  # bsdtar documentation
  SET(bsdtar_MANS bsdtar.1)

  # How to build bsdtar
  ADD_EXECUTABLE(bsdtar ${bsdtar_SOURCES})
  IF(ENABLE_TAR_SHARED)
    TARGET_LINK_LIBRARIES(bsdtar archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_TAR_SHARED)
    TARGET_LINK_LIBRARIES(bsdtar archive_static ${ADDITIONAL_LIBS})
    SET_TARGET_PROPERTIES(bsdtar PROPERTIES COMPILE_DEFINITIONS
    				 LIBARCHIVE_STATIC)
  ENDIF(ENABLE_TAR_SHARED)
  IF(WIN32 AND NOT CYGWIN AND MSVC)
    # MSVC merges .manifest sources into the linker's default manifest
    TARGET_SOURCES(bsdtar PRIVATE bsdtar.exe.manifest)
  ENDIF(WIN32 AND NOT CYGWIN AND MSVC)

  # Installation rules
  IF(ENABLE_INSTALL)
    INSTALL(TARGETS bsdtar RUNTIME DESTINATION bin)
    INSTALL_MAN(${bsdtar_MANS})
  ENDIF(ENABLE_INSTALL)
ENDIF(ENABLE_TAR)

add_subdirectory(test)
