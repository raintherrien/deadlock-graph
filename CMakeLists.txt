cmake_minimum_required(VERSION 3.13)
project(deadlock-graph VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to compile release
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Add warnings
# Note: add_compile_options propegates these options to e.g. examples.
set(DEADLOCK_GRAPH_MSVC_WARNFLAGS /W4 /WX)
set(DEADLOCK_GRAPH_OTHR_WARNFLAGS -Wall -Werror -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic)
add_compile_options("$<IF:$<C_COMPILER_ID:MSVC>,${DEADLOCK_GRAPH_MSVC_WARNFLAGS},${DEADLOCK_GRAPH_OTHR_WARNFLAGS}>")

set(DEADLOCK_GRAPH_SOURCES ${PROJECT_SOURCE_DIR}/src/dot.cpp
                           ${PROJECT_SOURCE_DIR}/src/flow.cpp
                           ${PROJECT_SOURCE_DIR}/src/graph.cpp
                           ${PROJECT_SOURCE_DIR}/src/main.cpp)
add_executable(deadlock-graph ${DEADLOCK_GRAPH_SOURCES})

option(DEADLOCK_GRAPH_BUILD_SAN "Build with sanitizers" OFF)
if(DEADLOCK_GRAPH_BUILD_SAN)
	target_compile_options(deadlock-graph PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
	target_link_options   (deadlock-graph PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
endif()
