#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AndTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502AndTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AndTest, AndImmediateMasksAccumulator) {
    ram.write(0x0000, 0x29);
    ram.write(0x0001, 0x3C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AndTest, AndLeavesCarryAndOverflowUntouched) {
    ram.write(0x0000, 0x29);
    ram.write(0x0001, 0xFF);
    cpu.reset();
    cpu.A(0x80);
    cpu.CFlag(true);
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502AndTest, AndSetsZeroWhenNoBitsOverlap) {
    ram.write(0x0000, 0x29);
    ram.write(0x0001, 0xF0);
    cpu.reset();
    cpu.A(0x0F);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502AndTest, EightTicksCompleteAndImmediate) {
    ram.write(0x0000, 0x29);
    ram.write(0x0001, 0x3C);
    cpu.reset();
    cpu.A(0xF0);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0xF0);

    cpu.tick();
    EXPECT_EQ(cpu.A(), 0x30);
}

TEST_F(CPU6502AndTest, AndZeroPageMasksFromMemory) {
    ram.write(0x0000, 0x25);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x3C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AndTest, AndZeroPageXMasksUsingIndexedAddress) {
    ram.write(0x0000, 0x35);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
}

TEST_F(CPU6502AndTest, AndAbsoluteMasksFromMemory) {
    ram.write(0x0000, 0x2D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x3C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AndTest, AndAbsoluteXMasksWithoutPageCrossing) {
    ram.write(0x0000, 0x3D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
}

TEST_F(CPU6502AndTest, AndAbsoluteXMasksAcrossPageCrossing) {
    ram.write(0x0000, 0x3D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
}

TEST_F(CPU6502AndTest, AndAbsoluteYMasksWithoutPageCrossing) {
    ram.write(0x0000, 0x39);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
}

TEST_F(CPU6502AndTest, AndIndirectXMasksThroughPointerTable) {
    ram.write(0x0000, 0x21);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AndTest, AndIndirectYMasksWithoutPageCrossing) {
    ram.write(0x0000, 0x31);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x3C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x30);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
