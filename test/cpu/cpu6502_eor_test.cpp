#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502EorTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502EorTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502EorTest, EorImmediateTogglesBits) {
    ram.write(0x0000, 0x49);
    ram.write(0x0001, 0xFF);
    cpu.reset();
    cpu.A(0x0F);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502EorTest, EorLeavesCarryAndOverflowUntouched) {
    ram.write(0x0000, 0x49);
    ram.write(0x0001, 0x5A);
    cpu.reset();
    cpu.A(0x5A);
    cpu.CFlag(true);
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.VFlag());
}

TEST_F(CPU6502EorTest, EightTicksCompleteEorImmediate) {
    ram.write(0x0000, 0x49);
    ram.write(0x0001, 0xFF);
    cpu.reset();
    cpu.A(0x0F);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x0F);

    cpu.tick();
    EXPECT_EQ(cpu.A(), 0xF0);
}

TEST_F(CPU6502EorTest, EorZeroPageTogglesFromMemory) {
    ram.write(0x0000, 0x45);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();
    cpu.A(0x0F);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502EorTest, EorZeroPageXTogglesUsingIndexedAddress) {
    ram.write(0x0000, 0x55);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
}

TEST_F(CPU6502EorTest, EorAbsoluteTogglesFromMemory) {
    ram.write(0x0000, 0x4D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0xFF);
    cpu.reset();
    cpu.A(0x0F);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502EorTest, EorAbsoluteXTogglesWithoutPageCrossing) {
    ram.write(0x0000, 0x5D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
}

TEST_F(CPU6502EorTest, EorAbsoluteXTogglesAcrossPageCrossing) {
    ram.write(0x0000, 0x5D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
}

TEST_F(CPU6502EorTest, EorAbsoluteYTogglesWithoutPageCrossing) {
    ram.write(0x0000, 0x59);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
}

TEST_F(CPU6502EorTest, EorIndirectXTogglesThroughPointerTable) {
    ram.write(0x0000, 0x41);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502EorTest, EorIndirectYTogglesWithoutPageCrossing) {
    ram.write(0x0000, 0x51);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0xFF);
    cpu.reset();
    cpu.A(0x0F);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0xF0);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
