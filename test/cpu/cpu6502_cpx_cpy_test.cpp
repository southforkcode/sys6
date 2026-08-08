#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502CpxCpyTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502CpxCpyTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502CpxCpyTest, CpxImmediateComparesXNotA) {
    ram.write(0x0000, 0xE0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.X(0x10);
    cpu.A(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x10); // CPX never writes X
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CpxCpyTest, CpxImmediateSetsZeroWhenEqual) {
    ram.write(0x0000, 0xE0);
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.X(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502CpxCpyTest, CpxZeroPageComparesFromMemory) {
    ram.write(0x0000, 0xE4);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.X(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CpxCpyTest, CpxAbsoluteComparesFromMemory) {
    ram.write(0x0000, 0xEC);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.X(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502CpxCpyTest, CpyImmediateComparesYNotA) {
    ram.write(0x0000, 0xC0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.Y(0x10);
    cpu.A(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x10); // CPY never writes Y
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CpxCpyTest, CpyImmediateClearsCarryWhenYLess) {
    ram.write(0x0000, 0xC0);
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502CpxCpyTest, CpyZeroPageComparesFromMemory) {
    ram.write(0x0000, 0xC4);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.Y(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502CpxCpyTest, CpyAbsoluteComparesFromMemory) {
    ram.write(0x0000, 0xCC);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.Y(0x10);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.PC(), 0x0003);
}
