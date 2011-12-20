############################################
#
# How to build bsdcpio
#
############################################
IF(ENABLE_CPIO)

  SET(bsdcpio_SOURCES
    cmdline.c
    cpio.c
    cpio.h
    cpio_platform.h
    ../libarchive_fe/err.c
    ../libarchive_fe/err.h
    ../libarchive_fe/lafe_platform.h
    ../libarchive_fe/line_reader.c
    ../libarchive_fe/line_reader.h
    ../libarchive_fe/matching.c
    ../libarchive_fe/matching.h
    ../libarchive_fe/pathmatch.c
    ../libarchive_fe/pathmatch.h
  )
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../libarchive_fe)
  IF(WIN32 AND NOT CYGWIN)
    LIST(APPEND bsdcpio_SOURCES cpio_windows.c)
    LIST(APPEND bsdcpio_SOURCES cpio_windows.h)
  ENDIF(WIN32 AND NOT CYGWIN)

  # bsdcpio documentation
  SET(bsdcpio_MANS bsdcpio.1)

  # How to build bsdcpio
  ADD_EXECUTABLE(bsdcpio ${bsdcpio_SOURCES})
  IF(ENABLE_CPIO_SHARED)
    TARGET_LINK_LIBRARIES(bsdcpio archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_CPIO_SHARED)
    TARGET_LINK_LIBRARIES(bsdcpio archive_static ${ADDITIONAL_LIBS})
    SET_TARGET_PROPERTIES(bsdcpio PROPERTIES COMPILE_DEFINITIONS
    				  LIBARCHIVE_STATIC)
  ENDIF(ENABLE_CPIO_SHARED)
  # Full path to the compiled executable (used by test suite)
  GET_TARGET_PROPERTY(BSDCPIO bsdcpio LOCATION)

  # Installation rules
  INSTALL(TARGETS bsdcpio RUNTIME DESTINATION bin)
  INSTALL_MAN(${bsdcpio_MANS})

ENDIF(ENABLE_CPIO)

# Test suite
add_subdirectory(test)
