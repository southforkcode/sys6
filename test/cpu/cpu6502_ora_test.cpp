#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502OraTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502OraTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502OraTest, OraImmediateCombinesAccumulator) {
    ram.write(0x0000, 0x09);
    ram.write(0x0001, 0x0C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502OraTest, OraLeavesCarryAndOverflowUntouched) {
    ram.write(0x0000, 0x09);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.A(0x00);
    cpu.CFlag(true);
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502OraTest, EightTicksCompleteOraImmediate) {
    ram.write(0x0000, 0x09);
    ram.write(0x0001, 0x0C);
    cpu.reset();
    cpu.A(0xF0);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0xF0);

    cpu.tick();
    EXPECT_EQ(cpu.A(), 0xFC);
}

TEST_F(CPU6502OraTest, OraZeroPageCombinesFromMemory) {
    ram.write(0x0000, 0x05);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x0C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502OraTest, OraZeroPageXCombinesUsingIndexedAddress) {
    ram.write(0x0000, 0x15);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
}

TEST_F(CPU6502OraTest, OraAbsoluteCombinesFromMemory) {
    ram.write(0x0000, 0x0D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x0C);
    cpu.reset();
    cpu.A(0xF0);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502OraTest, OraAbsoluteXCombinesWithoutPageCrossing) {
    ram.write(0x0000, 0x1D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
}

TEST_F(CPU6502OraTest, OraAbsoluteXCombinesAcrossPageCrossing) {
    ram.write(0x0000, 0x1D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
}

TEST_F(CPU6502OraTest, OraAbsoluteYCombinesWithoutPageCrossing) {
    ram.write(0x0000, 0x19);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
}

TEST_F(CPU6502OraTest, OraIndirectXCombinesThroughPointerTable) {
    ram.write(0x0000, 0x01);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502OraTest, OraIndirectYCombinesWithoutPageCrossing) {
    ram.write(0x0000, 0x11);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x0C);
    cpu.reset();
    cpu.A(0xF0);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xFC);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
