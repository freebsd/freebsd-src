############################################
#
# How to build bsdcat
#
############################################
IF(ENABLE_CAT)

  SET(bsdcat_SOURCES
    bsdcat.c
    bsdcat.h
    bsdcat_platform.h
    cmdline.c
    ../libarchive_fe/err.c
    ../libarchive_fe/err.h
    ../libarchive_fe/lafe_platform.h
  )
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../libarchive_fe)

  # bsdcat documentation
  SET(bsdcat_MANS bsdcat.1)

  # How to build bsdcat
  ADD_EXECUTABLE(bsdcat ${bsdcat_SOURCES})
  IF(ENABLE_CAT_SHARED)
    TARGET_LINK_LIBRARIES(bsdcat archive ${ADDITIONAL_LIBS})
  ELSE(ENABLE_CAT_SHARED)
    TARGET_LINK_LIBRARIES(bsdcat archive_static ${ADDITIONAL_LIBS})
    SET_TARGET_PROPERTIES(bsdcat PROPERTIES COMPILE_DEFINITIONS
    				 LIBARCHIVE_STATIC)
  ENDIF(ENABLE_CAT_SHARED)

  # Installation rules
  INSTALL(TARGETS bsdcat RUNTIME DESTINATION bin)
  INSTALL_MAN(${bsdcat_MANS})
ENDIF(ENABLE_CAT)

add_subdirectory(test)
