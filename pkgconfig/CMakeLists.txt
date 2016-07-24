## generate libdivsufsort.pc ##
set(W64BIT "")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libdivsufsort.pc.cmake" "${CMAKE_CURRENT_BINARY_DIR}/libdivsufsort.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdivsufsort.pc" DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})
if(BUILD_DIVSUFSORT64)
  set(W64BIT "64")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libdivsufsort.pc.cmake" "${CMAKE_CURRENT_BINARY_DIR}/libdivsufsort64.pc" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libdivsufsort64.pc" DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})
endif(BUILD_DIVSUFSORT64)
