include(GoogleTest)

add_executable(sys6_tests
    cpu/cpu6502_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
)

target_include_directories(sys6_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(sys6_tests PRIVATE GTest::gtest_main)

gtest_discover_tests(sys6_tests)
