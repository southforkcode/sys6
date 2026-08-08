#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AdcTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502AdcTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AdcTest, FirstTickAdvancesPCPastOpcodeOnly) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();

    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502AdcTest, AdcImmediateAddsOperandToAccumulator) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcImmediateIncludesIncomingCarry) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x16);
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsCarryAndZeroOnUnsignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0xFF);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsOverflowAndNegativeOnSignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0x7F);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x80);
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502AdcTest, AdcImmediateTakesExactlyTwoTicksToComplete) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick(); // cycle 0: fetch opcode only

    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // cycle 1: fetch operand, apply ALU, complete

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
