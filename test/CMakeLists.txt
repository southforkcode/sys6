include(GoogleTest)

add_executable(sys6_tests
    cpu/alu_test.cpp
    cpu/cpu6502_adc_test.cpp
    cpu/cpu6502_sbc_test.cpp
    cpu/cpu6502_and_test.cpp
    cpu/cpu6502_ora_test.cpp
    cpu/cpu6502_eor_test.cpp
    cpu/cpu6502_cmp_test.cpp
    cpu/cpu6502_cpx_cpy_test.cpp
    cpu/cpu6502_shift_test.cpp
    cpu/cpu6502_inc_dec_test.cpp
    cpu/cpu6502_load_store_test.cpp
    cpu/cpu6502_transfer_test.cpp
    cpu/cpu6502_branch_test.cpp
    cpu/cpu6502_brk_test.cpp
    cpu/cpu6502_fibonacci_e2e_test.cpp
    cpu/cpu6502_addressing_test.cpp
    cpu/cpu6502_clock_test.cpp
    cpu/cpu6502_test.cpp
    memory/ram_test.cpp
    memory/rom_test.cpp
    memory/bus_test.cpp
    utils/program_loader_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/alu.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/ram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/rom.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/bus.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/program_loader.cpp
)

target_include_directories(sys6_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(sys6_tests PRIVATE GTest::gtest_main)

gtest_discover_tests(sys6_tests)
