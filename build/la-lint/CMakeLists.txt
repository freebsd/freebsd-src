cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 23)

project(libarchive-clang-tools)
find_package(zstd REQUIRED)
find_package(Clang REQUIRED)
add_executable(la-lint)

target_sources(la-lint
	PRIVATE
		main.cpp
)

target_include_directories(la-lint
	PRIVATE
		${CLANG_INCLUDE_DIRS}
)

target_link_libraries(la-lint
	PRIVATE
	LLVM LLVMCore LLVMSupport
	libclang clang-cpp
)

install(TARGETS la-lint RUNTIME DESTINATION bin)
