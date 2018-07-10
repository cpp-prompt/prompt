cmake_minimum_required (VERSION 3.9)
MESSAGE(STATUS "CMAKE_ROOT: " ${CMAKE_ROOT})

# Project name
project(Cpp-Prompt)

# Turn on the verbose
set(CMAKE_VERBOSE_MAKEFILE ON)

# CXX target properties
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++fs")

message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER})
message(STATUS "CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS})
message(STATUS "CMAKE_EXE_LINKER_FLAGS: " ${CMAKE_EXE_LINKER_FLAGS})

# The version number
set (PROMPT_MAJOR_VERSION "0")
set (PROMPT_MINOR_VERSION "1")

# add the binary tree to the search path for include files
include_directories(${PROJECT_SOURCE_DIR})
include_directories(doctest)


# -----------------------------------------------------------------------------
# Example program 
# -----------------------------------------------------------------------------
message(STATUS "Building examples ...")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/example)

list(APPEND EXAMPLE_EXE_LINKER_FLAGS "")

message(STATUS "EXAMPLE_CXX_FLAGS: " ${EXAMPLE_CXX_FLAGS})
message(STATUS "EXAMPLE_EXE_LINKER_FLAGS: " ${EXAMPLE_EXE_LINKER_FLAGS})

add_executable(simple example/simple.cpp)
target_link_libraries(simple -lstdc++fs)


# -----------------------------------------------------------------------------
# Unittest
# -----------------------------------------------------------------------------
enable_testing()

message(STATUS "Building unit tests ...")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/unittest)
add_executable(radixtree unittest/radixtree.cpp)
target_link_libraries(radixtree -lstdc++fs)


add_test(RadixTree ${PROJECT_SOURCE_DIR}/unittest/radixtree -tc=RadixTree)

