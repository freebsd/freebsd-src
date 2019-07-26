
set(CXXRT_SOURCES
    dynamic_cast.cc
    exception.cc
    guard.cc
    stdexcept.cc
    typeinfo.cc
    memory.cc
    auxhelper.cc
    libelftc_dem_gnu3.c
   )

add_definitions(-D_GNU_SOURCE)
add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})
target_link_libraries(cxxrt-shared ${CMAKE_DL_LIBS})
target_link_libraries(cxxrt-static ${CMAKE_DL_LIBS})

set_target_properties(cxxrt-static cxxrt-shared PROPERTIES
                      OUTPUT_NAME "cxxrt"
                     )



