#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "utils/program_loader.h"

TEST(CPU6502FibonacciE2E, ComputesAndStoresFirstTenFibonacciNumbers) {
    RAM ram(0x10000);
    Bus bus;
    bus.attach(0x0000, 0xFFFF, ram);
    CPU6502 cpu(bus);

    const std::string program =
        "A9 00 85 F0"       // LDA #$00 ; STA $F0        (a = 0)
        "A9 01 85 F1"       // LDA #$01 ; STA $F1        (b = 1)
        "A9 0A 85 F3"       // LDA #$0A ; STA $F3        (counter = 10)
        "A5 F0"             // LOOP: LDA $F0             (A = a)
        "99 00 02"          //   STA $0200,Y             (output[Y] = a)
        "65 F1"             //   ADC $F1                 (A = a + b)
        "85 F2"             //   STA $F2                 (scratch = newB)
        "A5 F1 85 F0"       //   LDA $F1 ; STA $F0        (a = old b)
        "A5 F2 85 F1"       //   LDA $F2 ; STA $F1        (b = newB)
        "C8"                //   INY
        "C6 F3"             //   DEC $F3                 (counter--)
        "D0 EA"             //   BNE LOOP
        "00";               // BRK

    loadProgram(ram, 0x0000, program);
    cpu.reset();

    ASSERT_TRUE(cpu.run(10000));

    const uint8_t expected[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    for (uint16_t i = 0; i < 10; ++i) {
        EXPECT_EQ(ram.read(static_cast<uint16_t>(0x0200 + i)), expected[i]) << "index " << i;
    }
}
