# === Replicated MapReduce ===
cmake_minimum_required(VERSION 3.10)
project(MapReduceProject)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/common)
include_directories(/usr/local/include)  # General include directory
include_directories(/usr/local/include/libnuraft)  # Raft include directory

add_executable(mapreduce_server
               src/mapreduce_server.cpp
               src/KeyValueStore.cpp
               src/MapReduce.cpp
               src/common/logger.cc
               src/common/in_memory_log_store.cxx)

# Link NuRaft, OpenSSL, and other necessary libraries
find_package(OpenSSL REQUIRED)
target_link_libraries(mapreduce_server /usr/local/lib/libnuraft.a OpenSSL::SSL OpenSSL::Crypto)

# === MapReduce Tests ===
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)
add_executable(mapreduce_tests
            src/tests/keyvaluestore_tests.cpp
            src/tests/mapreduce_tests.cpp
            src/KeyValueStore.cpp
            src/MapReduce.cpp
               )
target_link_libraries(mapreduce_tests gtest_main)
target_include_directories(mapreduce_tests PUBLIC
                           ${YOUR_INCLUDE_DIRECTORIES})
enable_testing()
add_test(NAME mapreduce_test COMMAND mapreduce_tests)
