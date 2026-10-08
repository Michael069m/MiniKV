cmake_minimum_required(VERSION 3.14)
project(MiniRedis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE AND EXISTS "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/usr/include/c++/v1")
    include_directories(SYSTEM "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/usr/include/c++/v1")
    set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk")
endif()

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "Cannot enable ASan and TSan simultaneously")
endif()

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

include_directories(include)

# Core Library
add_library(miniredis_lib
    src/kvstore.cpp
    src/wal.cpp
    src/resp.cpp
)

# Driver Executable
add_executable(miniredis_server
    src/main.cpp
)
target_link_libraries(miniredis_server PRIVATE miniredis_lib)

# Unit Testing via GoogleTest
enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    tests/test_kvstore.cpp
    tests/test_wal.cpp
    tests/test_resp.cpp
)
target_link_libraries(unit_tests PRIVATE miniredis_lib gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
