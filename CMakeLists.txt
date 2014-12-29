
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


add_library(cxxrt-static STATIC ${CXXRT_SOURCES})
add_library(cxxrt-shared SHARED ${CXXRT_SOURCES})

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(cxxrt-shared dl)
    target_link_libraries(cxxrt-static dl)
endif()

set_target_properties(cxxrt-static cxxrt-shared PROPERTIES
                      OUTPUT_NAME "cxxrt"
                     )



