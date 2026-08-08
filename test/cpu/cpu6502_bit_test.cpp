#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502BitTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502BitTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502BitTest, BitZeroPageSetsZeroFlagWhenAndResultIsZero) {
    ram.write(0x0000, 0x24); // BIT zp
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x0F);
    cpu.reset();
    cpu.A(0xF0); // 0xF0 & 0x0F == 0

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageClearsZeroFlagWhenAndResultIsNonzero) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();
    cpu.A(0x01); // 0x01 & 0xFF == 0x01

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageSetsNegativeFlagFromOperandBit7RegardlessOfAndResult) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x80); // bit 7 set
    cpu.reset();
    cpu.A(0x00); // AND result is 0, but N still follows the operand's bit 7

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageClearsNegativeFlagWhenOperandBit7Clear) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x7F);
    cpu.reset();
    cpu.A(0xFF);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageSetsOverflowFlagFromOperandBit6RegardlessOfAndResult) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x40); // bit 6 set
    cpu.reset();
    cpu.A(0x00);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.VFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageClearsOverflowFlagWhenOperandBit6Clear) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xBF);
    cpu.reset();
    cpu.A(0xFF);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.VFlag());
}

TEST_F(CPU6502BitTest, BitZeroPageDoesNotModifyAccumulator) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();
    cpu.A(0x42);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BitTest, BitZeroPageDoesNotAffectCarryFlag) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502BitTest, ThreeTicksCompleteBitZeroPage) {
    ram.write(0x0000, 0x24);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    cpu.reset();

    for (int step = 0; step < 2; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502BitTest, BitAbsoluteSetsFlagsFromMemory) {
    ram.write(0x0000, 0x2C); // BIT abs
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // 0x0200
    ram.write(0x0200, 0xC0); // bits 7 and 6 set
    cpu.reset();
    cpu.A(0xFF);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502BitTest, FourTicksCompleteBitAbsolute) {
    ram.write(0x0000, 0x2C);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x00);
    cpu.reset();
    cpu.A(0xFF);
    cpu.ZFlag(false);

    for (int step = 0; step < 3; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_FALSE(cpu.ZFlag()); // not yet committed

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_TRUE(cpu.ZFlag());
}
