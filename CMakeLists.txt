cmake_minimum_required(VERSION 3.20)
project(sys6 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(sys6
    src/main.cpp
    src/cpu/cpu6502.cpp
    src/memory/ram.cpp
    src/memory/rom.cpp
    src/memory/bus.cpp
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

find_program(CLANG_FORMAT_EXE NAMES clang-format HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FORMAT_SOURCE_FILES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/test/*.cpp
        ${CMAKE_SOURCE_DIR}/test/*.h
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCE_FILES}
        COMMENT "Running clang-format on all source files"
        VERBATIM
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_EXTRA_ARGS "")
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        list(APPEND CLANG_TIDY_EXTRA_ARGS --extra-arg=-isysroot --extra-arg=${MACOS_SDK_PATH})
    endif()
    file(GLOB_RECURSE LINT_SOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
    if(BUILD_TESTING)
        file(GLOB_RECURSE LINT_TEST_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*.cpp)
        list(APPEND LINT_SOURCE_FILES ${LINT_TEST_FILES})
    endif()
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${CLANG_TIDY_EXTRA_ARGS} ${LINT_SOURCE_FILES}
        COMMENT "Running clang-tidy on all source files"
        VERBATIM
    )
endif()
