set(SOURCES cbor.c cbor/streaming.c cbor/internal/encoders.c cbor/internal/builder_callbacks.c cbor/internal/loaders.c cbor/internal/memory_utils.c cbor/internal/stack.c cbor/internal/unicode.c cbor/encoding.c cbor/serialization.c cbor/arrays.c cbor/common.c cbor/floats_ctrls.c cbor/bytestrings.c cbor/callbacks.c cbor/strings.c cbor/maps.c cbor/tags.c cbor/ints.c)

include(GNUInstallDirs)
set(CMAKE_SKIP_BUILD_RPATH FALSE)

if (CBOR_CUSTOM_ALLOC)
    LIST(APPEND SOURCES allocators.c)
endif(CBOR_CUSTOM_ALLOC)

if (NOT DEFINED CMAKE_MACOSX_RPATH)
	set(CMAKE_MACOSX_RPATH 0)
endif()

add_library(cbor ${SOURCES})
target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include(GenerateExportHeader)
generate_export_header(cbor EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/cbor/cbor_export.h)
target_include_directories(cbor PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cbor/cbor_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbor)

if (NOT ${CBOR_VERSION_MAJOR} EQUAL 0)
	MESSAGE(FATAL_ERROR "Change the shared library version scheme to reflect https://github.com/PJK/libcbor/issues/52.")
endif()

set_target_properties(cbor PROPERTIES
		VERSION ${CBOR_VERSION}
		MACHO_COMPATIBILITY_VERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR}.0
		SOVERSION ${CBOR_VERSION_MAJOR}.${CBOR_VERSION_MINOR})

configure_file(libcbor.pc.in libcbor.pc @ONLY)

# http://www.cmake.org/Wiki/CMake:Install_Commands
install(TARGETS cbor
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY cbor DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                FILES_MATCHING PATTERN "*.h")

install(FILES cbor.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libcbor.pc"
	DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig")
