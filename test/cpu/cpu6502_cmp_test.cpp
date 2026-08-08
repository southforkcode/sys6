#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502CmpTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502CmpTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502CmpTest, CmpImmediateSetsCarryAndClearsZeroWhenAccGreater) {
    ram.write(0x0000, 0xC9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x10); // CMP never writes A
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CmpTest, CmpImmediateSetsZeroAndCarryWhenEqual) {
    ram.write(0x0000, 0xC9);
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.A(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CmpTest, CmpImmediateClearsCarryWhenAccLess) {
    ram.write(0x0000, 0xC9);
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.A(0x05);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502CmpTest, CmpLeavesOverflowUntouched) {
    ram.write(0x0000, 0xC9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.VFlag());
}

TEST_F(CPU6502CmpTest, EightTicksCompleteCmpImmediate) {
    ram.write(0x0000, 0xC9);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_FALSE(cpu.CFlag());

    cpu.tick();
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CmpTest, CmpZeroPageComparesFromMemory) {
    ram.write(0x0000, 0xC5);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CmpTest, CmpZeroPageXComparesUsingIndexedAddress) {
    ram.write(0x0000, 0xD5);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x10);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502CmpTest, CmpAbsoluteComparesFromMemory) {
    ram.write(0x0000, 0xCD);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502CmpTest, CmpAbsoluteXComparesWithoutPageCrossing) {
    ram.write(0x0000, 0xDD);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CmpTest, CmpAbsoluteXComparesAcrossPageCrossing) {
    ram.write(0x0000, 0xDD);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CmpTest, CmpAbsoluteYComparesWithoutPageCrossing) {
    ram.write(0x0000, 0xD9);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CmpTest, CmpIndirectXComparesThroughPointerTable) {
    ram.write(0x0000, 0xC1);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CmpTest, CmpIndirectYComparesWithoutPageCrossing) {
    ram.write(0x0000, 0xD1);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}
