#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502SbcTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502SbcTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502SbcTest, SbcImmediateSubtractsOperandWhenCarrySet) {
    ram.write(0x0000, 0xE9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502SbcTest, SbcImmediateBorrowsWhenCarryClear) {
    ram.write(0x0000, 0xE9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0A);
}

TEST_F(CPU6502SbcTest, SbcImmediateClearsCarryOnUnsignedUnderflow) {
    ram.write(0x0000, 0xE9);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0x00);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFF);
    EXPECT_FALSE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502SbcTest, EightTicksCompleteSbcImmediate) {
    ram.write(0x0000, 0xE9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcZeroPageSubtractsOperandFromMemory) {
    ram.write(0x0000, 0xE5);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502SbcTest, TwelveTicksCompleteSbcZeroPage) {
    ram.write(0x0000, 0xE5);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcZeroPageXSubtractsOperandUsingIndexedAddress) {
    ram.write(0x0000, 0xF5);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502SbcTest, SbcAbsoluteSubtractsOperandFromMemory) {
    ram.write(0x0000, 0xED);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502SbcTest, SixteenTicksCompleteSbcAbsolute) {
    ram.write(0x0000, 0xED);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcAbsoluteXSubtractsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0xFD);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502SbcTest, SbcAbsoluteXSubtractsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0xFD);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, TwentyTicksCompleteSbcAbsoluteXWithPageCrossing) {
    ram.write(0x0000, 0xFD);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcAbsoluteYSubtractsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0xF9);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502SbcTest, SbcAbsoluteYSubtractsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0xF9);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcIndirectXSubtractsOperandThroughPointerTable) {
    ram.write(0x0000, 0xE1);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502SbcTest, TwentyFourTicksCompleteSbcIndirectX) {
    ram.write(0x0000, 0xE1);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(true);

    for (int i = 0; i < 24; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.A(), 0x0B);
}

TEST_F(CPU6502SbcTest, SbcIndirectYSubtractsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0xF1);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502SbcTest, SbcIndirectYSubtractsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0xF1);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    ram.write(0x0011, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x0B);
}
