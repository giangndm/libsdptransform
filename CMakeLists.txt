cmake_minimum_required(VERSION 3.0)

project(sdptransform VERSION 1.2.4)

# For CMake >= 3.1.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# For CMake < 3.1.
add_compile_options(-std=c++11)

subdirs(test readme-helper)
add_subdirectory(./deps)
include_directories(${sdptransform_SOURCE_DIR}/include)
include_directories(${sdptransform_SOURCE_DIR}/deps/re2/)

set(
	SOURCE_FILES
	src/grammar.cpp
	src/parser.cpp
	src/writer.cpp
)

set(
	HEADER_FILES
	include/sdptransform.hpp
	include/json.hpp
)

add_library(sdptransform STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(sdptransform re2)

install(TARGETS sdptransform DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/sdptransform)
