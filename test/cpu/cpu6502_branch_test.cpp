#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502BranchTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502BranchTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502BranchTest, BeqBranchesWhenZeroFlagSet) {
    ram.write(0x0000, 0xF0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BeqDoesNotBranchWhenZeroFlagClear) {
    ram.write(0x0000, 0xF0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BneBranchesWhenZeroFlagClear) {
    ram.write(0x0000, 0xD0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BneDoesNotBranchWhenZeroFlagSet) {
    ram.write(0x0000, 0xD0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BcsBranchesWhenCarryFlagSet) {
    ram.write(0x0000, 0xB0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BcsDoesNotBranchWhenCarryFlagClear) {
    ram.write(0x0000, 0xB0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BccBranchesWhenCarryFlagClear) {
    ram.write(0x0000, 0x90);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BccDoesNotBranchWhenCarryFlagSet) {
    ram.write(0x0000, 0x90);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BmiBranchesWhenNegativeFlagSet) {
    ram.write(0x0000, 0x30);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BmiDoesNotBranchWhenNegativeFlagClear) {
    ram.write(0x0000, 0x30);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BplBranchesWhenNegativeFlagClear) {
    ram.write(0x0000, 0x10);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BplDoesNotBranchWhenNegativeFlagSet) {
    ram.write(0x0000, 0x10);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BvsBranchesWhenOverflowFlagSet) {
    ram.write(0x0000, 0x70);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BvsDoesNotBranchWhenOverflowFlagClear) {
    ram.write(0x0000, 0x70);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BvcBranchesWhenOverflowFlagClear) {
    ram.write(0x0000, 0x50);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BvcDoesNotBranchWhenOverflowFlagSet) {
    ram.write(0x0000, 0x50);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, NegativeOffsetBranchesBackward) {
    ram.write(0x0010, 0xF0); // BEQ
    ram.write(0x0011, static_cast<uint8_t>(-5));
    cpu.reset();
    cpu.PC(0x0010);
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x000D); // 0x0012 - 5
}

TEST_F(CPU6502BranchTest, NotTakenBranchCompletesInTwoCycles) {
    ram.write(0x0000, 0xF0); // BEQ
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, TakenSamePageBranchCompletesInThreeCycles) {
    ram.write(0x0000, 0xF0); // BEQ
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1: operand consumed, target not yet committed
    }
    EXPECT_EQ(cpu.PC(), 0x0002);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T2: target committed (no page cross)
    }

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, TakenPageCrossedBranchCompletesInFourCycles) {
    ram.write(0x00FD, 0xF0); // BEQ at 0x00FD
    ram.write(0x00FE, 0x05); // operand +5 from 0x00FF -> target 0x0104, crosses page
    cpu.reset();
    cpu.PC(0x00FD);
    cpu.ZFlag(true);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T2: page crossed, target not yet committed
    }
    EXPECT_EQ(cpu.PC(), 0x00FF);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T3: target committed
    }

    EXPECT_EQ(cpu.PC(), 0x0104);
}
