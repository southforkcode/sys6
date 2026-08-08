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

TEST_F(CPU6502AdcTest, AdcImmediateDoesNotUpdateAUntilFinalPhaseOfLastCycle) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete

    cpu.tick(); // T1 phase 1/4: Low -> LowToHigh
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 2/4: capture operand + ALU inputs on arrival at High
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 3/4: High -> HighToLow, ALU settles
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 4/4: commit -- A and flags updated
    EXPECT_EQ(cpu.A(), 0x15);
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

TEST_F(CPU6502AdcTest, AdcAbsoluteDoesNotUpdateAUntilFinalPhaseOfLastCycle) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete
    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1 complete
    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T2 complete

    cpu.tick(); // T3 phase 1/4
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 2/4: read operand from memory, load ALU inputs
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 3/4: ALU settles
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 4/4: commit -- A and flags updated
    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcZeroPageAddsOperandFromMemory) {
    ram.write(0x0000, 0x65);
    ram.write(0x0001, 0x10); // zero page address
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, TwelveTicksCompleteAdcZeroPageWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x65);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    for (int i = 0; i < 8; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcZeroPageXAddsOperandUsingIndexedAddress) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05); // effective address 0x10 + X(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcZeroPageXWrapsWithinZeroPage) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0xFF);
    ram.write(0x0004, 0x07); // effective address wraps: (0xFF + 0x05) & 0xFF = 0x04
    cpu.reset();
    cpu.A(0x01);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x08);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcZeroPageXWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteXAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + X(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteXAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02); // base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + X(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteXWithoutPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T3 completes the instruction: no page crossing

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcAbsoluteXWithPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    // T0-T3 complete: T3 was the idle page-crossing fixup cycle, A unchanged
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T4 completes the instruction

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteYAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteYAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02); // base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + Y(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteYWithoutPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcAbsoluteYWithPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectXAddsOperandThroughPointerTable) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0x10); // bb
    ram.write(0x0015, 0x00); // (bb + X) = 0x15 -> pointer low byte
    ram.write(0x0016, 0x02); // pointer high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcIndirectXWrapsPointerTableAddressWithinZeroPage) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0xFF); // bb
    ram.write(0x0004, 0x00); // (0xFF + 0x05) & 0xFF = 0x04 -> pointer low byte
    ram.write(0x0005, 0x02); // pointer high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectXWrapsPointerHighByteWithinZeroPage) {
    ram.write(0x0300, 0x61);
    ram.write(0x0301, 0xFD); // bb
    ram.write(0x00FF, 0x00); // (bb + X) & 0xFF = 0xFF -> pointer low byte
    ram.write(0x0000, 0x02); // pointer high byte wraps to zero page address 0x00 -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.PC(0x0300);
    cpu.A(0x10);
    cpu.X(0x02);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyFourTicksCompleteAdcIndirectXWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectYAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10); // bb
    ram.write(0x0010, 0x00); // pointer low byte
    ram.write(0x0011, 0x02); // pointer high byte -> base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcIndirectYAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF); // pointer low byte
    ram.write(0x0011, 0x02); // pointer high byte -> base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + Y(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectYWrapsPointerHighByteWithinZeroPage) {
    ram.write(0x0300, 0x71);
    ram.write(0x0301, 0xFF); // bb
    ram.write(0x00FF, 0x00); // pointer low byte
    ram.write(0x0000, 0x02); // pointer high byte wraps to zero page address 0x00 -> base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.PC(0x0300);
    cpu.A(0x10);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcIndirectYWithoutPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyFourTicksCompleteAdcIndirectYWithPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    ram.write(0x0011, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
