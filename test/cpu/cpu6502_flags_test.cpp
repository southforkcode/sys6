#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502FlagsTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502FlagsTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502FlagsTest, ClcClearsCarryFlag) {
    ram.write(0x0000, 0x18); // CLC
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502FlagsTest, ClcLeavesOtherFlagsUntouched) {
    ram.write(0x0000, 0x18); // CLC
    cpu.reset();
    cpu.CFlag(true);
    cpu.ZFlag(true);
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502FlagsTest, SecSetsCarryFlag) {
    ram.write(0x0000, 0x38); // SEC
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502FlagsTest, CliClearsInterruptDisableFlag) {
    ram.write(0x0000, 0x58); // CLI
    cpu.reset();
    cpu.IFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.IFlag());
}

TEST_F(CPU6502FlagsTest, SeiSetsInterruptDisableFlag) {
    ram.write(0x0000, 0x78); // SEI
    cpu.reset();
    cpu.IFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.IFlag());
}

TEST_F(CPU6502FlagsTest, CldClearsDecimalFlag) {
    ram.write(0x0000, 0xD8); // CLD
    cpu.reset();
    cpu.DFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.DFlag());
}

TEST_F(CPU6502FlagsTest, SedSetsDecimalFlag) {
    ram.write(0x0000, 0xF8); // SED
    cpu.reset();
    cpu.DFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.DFlag());
}

TEST_F(CPU6502FlagsTest, ClvClearsOverflowFlag) {
    ram.write(0x0000, 0xB8); // CLV
    cpu.reset();
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.VFlag());
}

TEST_F(CPU6502FlagsTest, EightTicksCompleteClc) {
    ram.write(0x0000, 0x18); // CLC
    cpu.reset();
    cpu.CFlag(true);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_TRUE(cpu.CFlag());

    cpu.tick();
    EXPECT_FALSE(cpu.CFlag());
}
