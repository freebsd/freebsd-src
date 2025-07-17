file(GLOB derdump_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_executable(derdump ${derdump_SOURCES})

target_include_directories(derdump PRIVATE "${CMAKE_SOURCE_DIR}/libder")
target_link_libraries(derdump der_static)
