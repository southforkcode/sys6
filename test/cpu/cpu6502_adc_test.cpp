#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AdcTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502AdcTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AdcTest, FourTicksAdvancePCPastOpcodeOnly) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502AdcTest, AdcImmediateAddsOperandToAccumulator) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcImmediateIncludesIncomingCarry) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x16);
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsCarryAndZeroOnUnsignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0xFF);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsOverflowAndNegativeOnSignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0x7F);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x80);
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502AdcTest, EightTicksCompleteAdcImmediate) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete: opcode fetched, PC -> 1

    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1 complete: operand fetched, ALU applied, PC -> 2

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteAddsOperandFromMemory) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00); // address low byte
    ram.write(0x0002, 0x02); // address high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteAssemblesAddressLowByteFirst) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x34); // address low byte
    ram.write(0x0002, 0x12); // address high byte -> effective address 0x1234
    ram.write(0x1234, 0x01);
    cpu.reset();
    cpu.A(0x01);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x02);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0: fetch opcode, PC -> 1
    EXPECT_EQ(cpu.PC(), 0x0001);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1: fetch address low byte, PC -> 2
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T2: fetch address high byte, PC -> 3
    EXPECT_EQ(cpu.PC(), 0x0003);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T3: read operand from memory, apply ALU

    EXPECT_EQ(cpu.A(), 0x15);
}
