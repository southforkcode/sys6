include(GoogleTest)

add_executable(sys6_tests
    cpu/alu_test.cpp
    cpu/cpu6502_adc_test.cpp
    cpu/cpu6502_addressing_test.cpp
    cpu/cpu6502_clock_test.cpp
    cpu/cpu6502_test.cpp
    memory/ram_test.cpp
    memory/rom_test.cpp
    memory/bus_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/alu.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/ram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/rom.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/bus.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
)

target_include_directories(sys6_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(sys6_tests PRIVATE GTest::gtest_main)

gtest_discover_tests(sys6_tests)
