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

    // Standard 6502 idiom: registers are undefined out of reset on real
    // hardware, so a program that depends on Y starting at 0 initializes it
    // explicitly (LDA #$00 ; TAY) rather than relying on this emulator's
    // reset() convenience-zeroing A/X/Y. The a<->b swap also holds newB in X
    // (ADC ; TAX ... TXA ; STA $F1) instead of a $F2 scratch byte -- using a
    // spare register as a temporary is the textbook alternative to burning
    // zero-page storage for single-instruction-lifetime values.
    const std::string program =
        "A9 00 A8"          // LDA #$00 ; TAY            (Y = 0, explicit)
        "85 F0"             // STA $F0                   (a = 0)
        "A9 01 85 F1"       // LDA #$01 ; STA $F1        (b = 1)
        "A9 0A 85 F3"       // LDA #$0A ; STA $F3        (counter = 10)
        "A5 F0"             // LOOP: LDA $F0             (A = a)
        "99 00 02"          //   STA $0200,Y             (output[Y] = a)
        "65 F1"             //   ADC $F1                 (A = a + b = newB)
        "AA"                //   TAX                     (X = newB)
        "A5 F1 85 F0"       //   LDA $F1 ; STA $F0        (a = old b)
        "8A"                //   TXA                     (A = newB)
        "85 F1"             //   STA $F1                 (b = newB)
        "C8"                //   INY
        "C6 F3"             //   DEC $F3                 (counter--)
        "D0 EC"             //   BNE LOOP
        "00";               // BRK

    loadProgram(ram, 0x0000, program);
    cpu.reset();

    ASSERT_TRUE(cpu.run(10000));

    const uint8_t expected[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    for (uint16_t i = 0; i < 10; ++i) {
        EXPECT_EQ(ram.read(static_cast<uint16_t>(0x0200 + i)), expected[i]) << "index " << i;
    }
}
