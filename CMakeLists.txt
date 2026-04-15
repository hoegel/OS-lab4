cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(lab4 LANGUAGES CXX)
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_COMPILER "/usr/bin/x86_64-w64-mingw32-g++")
endif()

include(FetchContent)
FetchContent_Declare(
    doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.zip
)
FetchContent_MakeAvailable(doctest)

add_executable(lab4_sync src/main.cpp)

add_executable(tests tests/test_sync.cpp)
target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${doctest_SOURCE_DIR}
)

enable_testing()
add_test(NAME ThreadTests COMMAND tests)