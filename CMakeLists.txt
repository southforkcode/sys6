cmake_minimum_required(VERSION 3.20)
project(sys6 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(sys6
    src/main.cpp
    src/cpu/cpu6502.cpp
    src/utils/log.cpp
)

target_include_directories(sys6 PRIVATE src)

option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(test)
endif()
