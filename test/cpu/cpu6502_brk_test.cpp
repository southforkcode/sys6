#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502BrkTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502BrkTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502BrkTest, BrkSetsHaltedAfterExecuting) {
    ram.write(0x0000, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();

    EXPECT_FALSE(cpu.halted());

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, BrkPushesReturnAddressAndStatusOntoStack) {
    ram.write(0x0200, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.P(0x20); // only the always-set bit 5; I and B both clear

    cpu.executeInstruction();

    // BRK reads a padding byte after its opcode, so the pushed return
    // address is PC + 2 = 0x0202.
    EXPECT_EQ(ram.read(0x01FF), 0x02); // PCH
    EXPECT_EQ(ram.read(0x01FE), 0x02); // PCL
    EXPECT_EQ(ram.read(0x01FD), 0x30); // P with B (0x10) and bit5 (0x20) set
    EXPECT_EQ(cpu.SP(), 0xFC);
}

TEST_F(CPU6502BrkTest, BrkSetsInterruptDisableFlag) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();
    cpu.IFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.IFlag());
}

TEST_F(CPU6502BrkTest, BrkLoadsPCFromBrkVector) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x34);
    ram.write(0xFFFF, 0x12);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502BrkTest, SevenTicksCompleteBrk) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x34);
    ram.write(0xFFFF, 0x12);
    cpu.reset();

    for (int step = 0; step < 7; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, ResetClearsHaltedFlagAfterBrk) {
    ram.write(0x0000, 0x00); // BRK
    cpu.reset();
    cpu.executeInstruction();
    ASSERT_TRUE(cpu.halted());

    cpu.reset();

    EXPECT_FALSE(cpu.halted());
}

TEST_F(CPU6502BrkTest, RunExecutesUntilHaltedAndReturnsTrue) {
    ram.write(0x0000, 0xE8); // INX
    ram.write(0x0001, 0xE8); // INX
    ram.write(0x0002, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();

    bool halted = cpu.run(100);

    EXPECT_TRUE(halted);
    EXPECT_EQ(cpu.X(), 2);
    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, RunReturnsFalseWhenInstructionCapReachedWithoutHalting) {
    ram.write(0x0000, 0x10); // BPL -2: unconditional infinite loop (N flag stays clear)
    ram.write(0x0001, static_cast<uint8_t>(-2));
    cpu.reset();

    bool halted = cpu.run(10);

    EXPECT_FALSE(halted);
    EXPECT_FALSE(cpu.halted());
}
