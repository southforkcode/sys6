#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502ShiftTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502ShiftTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502ShiftTest, AslAccumulatorShiftsLeftAndCarriesOutBit7) {
    ram.write(0x0000, 0x0A);
    cpu.reset();
    cpu.A(0x81);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x02);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502ShiftTest, TwoTicksCompleteAslAccumulator) {
    ram.write(0x0000, 0x0A);
    cpu.reset();
    cpu.A(0x40);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x40);

    cpu.tick();
    EXPECT_EQ(cpu.A(), 0x80);
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502ShiftTest, AslZeroPageWritesShiftedValueBackToMemory) {
    ram.write(0x0000, 0x06);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x81);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x02);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502ShiftTest, TwentyTicksCompleteAslZeroPage) {
    ram.write(0x0000, 0x06);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x81);
    cpu.reset();

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0010), 0x02);
}

TEST_F(CPU6502ShiftTest, AslZeroPageXWritesShiftedValueUsingIndexedAddress) {
    ram.write(0x0000, 0x16);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x40);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0015), 0x80);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502ShiftTest, TwentyFourTicksCompleteAslZeroPageX) {
    ram.write(0x0000, 0x16);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x40);
    cpu.reset();
    cpu.X(0x05);

    for (int i = 0; i < 24; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0015), 0x80);
}

TEST_F(CPU6502ShiftTest, AslAbsoluteWritesShiftedValueBackToMemory) {
    ram.write(0x0000, 0x0E);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x81);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0200), 0x02);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502ShiftTest, TwentyFourTicksCompleteAslAbsolute) {
    ram.write(0x0000, 0x0E);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x81);
    cpu.reset();

    for (int i = 0; i < 24; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0200), 0x02);
}

TEST_F(CPU6502ShiftTest, AslAbsoluteXWritesShiftedValueUsingIndexedAddress) {
    ram.write(0x0000, 0x1E);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x81);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0205), 0x02);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502ShiftTest, TwentyEightTicksCompleteAslAbsoluteXRegardlessOfPageCrossing) {
    // RMW absolute,X is a fixed 7 cycles on real hardware, unlike the
    // read-only ALU family where the extra cycle is conditional.
    ram.write(0x0000, 0x1E);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x81);
    cpu.reset();
    cpu.X(0x05);

    for (int i = 0; i < 28; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0304), 0x02);
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502ShiftTest, LsrAccumulatorShiftsRightAndCarriesOutBit0) {
    ram.write(0x0000, 0x4A);
    cpu.reset();
    cpu.A(0x01);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502ShiftTest, LsrZeroPageWritesShiftedValueBackToMemory) {
    ram.write(0x0000, 0x46);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x7F);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502ShiftTest, RolAccumulatorBringsInCarry) {
    ram.write(0x0000, 0x2A);
    cpu.reset();
    cpu.A(0x01);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x03);
    EXPECT_FALSE(cpu.CFlag());
}

TEST_F(CPU6502ShiftTest, RolAccumulatorCarriesOutBit7) {
    ram.write(0x0000, 0x2A);
    cpu.reset();
    cpu.A(0x80);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502ShiftTest, RolZeroPageWritesRotatedValueBackToMemory) {
    ram.write(0x0000, 0x26);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x80);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x01);
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502ShiftTest, RorAccumulatorBringsInCarryAtBit7) {
    ram.write(0x0000, 0x6A);
    cpu.reset();
    cpu.A(0x01);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x80);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502ShiftTest, RorZeroPageWritesRotatedValueBackToMemory) {
    ram.write(0x0000, 0x66);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x02);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0010), 0x01);
    EXPECT_FALSE(cpu.CFlag());
}
