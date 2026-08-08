#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502IncDecTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502IncDecTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502IncDecTest, IncZeroPageAddsOneToMemory) {
    ram.write(0x0000, 0xE6);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x06);
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502IncDecTest, IncZeroPageWrapsToZeroAndSetsZeroFlagWithoutTouchingCarry) {
    ram.write(0x0000, 0xE6);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x00);
    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.CFlag()); // INC never touches C
}

TEST_F(CPU6502IncDecTest, TwentyTicksCompleteIncZeroPage) {
    ram.write(0x0000, 0xE6);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0010), 0x06);
}

TEST_F(CPU6502IncDecTest, IncZeroPageXUsesIndexedAddress) {
    ram.write(0x0000, 0xF6);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0015), 0x06);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502IncDecTest, IncAbsoluteAddsOneToMemory) {
    ram.write(0x0000, 0xEE);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x7F);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0200), 0x80);
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502IncDecTest, IncAbsoluteXUsesIndexedAddress) {
    ram.write(0x0000, 0xFE);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0205), 0x06);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502IncDecTest, TwentyEightTicksCompleteIncAbsoluteXRegardlessOfPageCrossing) {
    ram.write(0x0000, 0xFE);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.X(0x05);

    for (int i = 0; i < 28; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0304), 0x06);
}

TEST_F(CPU6502IncDecTest, DecZeroPageSubtractsOneFromMemory) {
    ram.write(0x0000, 0xC6);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x04);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502IncDecTest, DecZeroPageWrapsBelowZeroWithoutTouchingCarry) {
    ram.write(0x0000, 0xC6);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0xFF);
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.CFlag());
}

TEST_F(CPU6502IncDecTest, DecZeroPageXUsesIndexedAddress) {
    ram.write(0x0000, 0xD6);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0015), 0x04);
}

TEST_F(CPU6502IncDecTest, DecAbsoluteSubtractsOneFromMemory) {
    ram.write(0x0000, 0xCE);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0200), 0x04);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502IncDecTest, DecAbsoluteXUsesIndexedAddress) {
    ram.write(0x0000, 0xDE);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0205), 0x04);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502IncDecTest, InxIncrementsXWithoutTouchingCarry) {
    ram.write(0x0000, 0xE8);
    cpu.reset();
    cpu.X(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x06);
    EXPECT_EQ(cpu.PC(), 0x0001);
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502IncDecTest, InxWrapsToZeroAndSetsZeroFlag) {
    ram.write(0x0000, 0xE8);
    cpu.reset();
    cpu.X(0xFF);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x00);
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502IncDecTest, EightTicksCompleteInx) {
    ram.write(0x0000, 0xE8);
    cpu.reset();
    cpu.X(0x05);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.X(), 0x05);

    cpu.tick();
    EXPECT_EQ(cpu.X(), 0x06);
}

TEST_F(CPU6502IncDecTest, DexDecrementsX) {
    ram.write(0x0000, 0xCA);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x04);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502IncDecTest, DexWrapsBelowZeroAndSetsNegativeFlag) {
    ram.write(0x0000, 0xCA);
    cpu.reset();
    cpu.X(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0xFF);
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502IncDecTest, InyIncrementsYWithoutTouchingA) {
    ram.write(0x0000, 0xC8);
    cpu.reset();
    cpu.Y(0x05);
    cpu.A(0x42);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x06);
    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502IncDecTest, DeyDecrementsY) {
    ram.write(0x0000, 0x88);
    cpu.reset();
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x04);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502IncDecTest, DeyWrapsBelowZeroAndSetsNegativeFlag) {
    ram.write(0x0000, 0x88);
    cpu.reset();
    cpu.Y(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0xFF);
    EXPECT_TRUE(cpu.NFlag());
}
