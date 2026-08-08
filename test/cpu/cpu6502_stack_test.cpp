#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502StackTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502StackTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502StackTest, PhaPushesAccumulatorOntoStack) {
    ram.write(0x0200, 0x48); // PHA
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.A(0x42);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x01FF), 0x42);
}

TEST_F(CPU6502StackTest, PhaDecrementsStackPointer) {
    ram.write(0x0200, 0x48); // PHA
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.SP(), 0xFE);
}

TEST_F(CPU6502StackTest, PhaThreeTicksCompleteInstruction) {
    ram.write(0x0200, 0x48); // PHA
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.A(0x42);

    for (int step = 0; step < 3; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(ram.read(0x01FF), 0x42);
    EXPECT_EQ(cpu.SP(), 0xFE);
}

TEST_F(CPU6502StackTest, PlaPullsAccumulatorFromStack) {
    ram.write(0x0200, 0x68); // PLA
    ram.write(0x01FF, 0x42);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x42);
}

TEST_F(CPU6502StackTest, PlaIncrementsStackPointer) {
    ram.write(0x0200, 0x68); // PLA
    ram.write(0x01FF, 0x42);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502StackTest, PlaSetsZeroFlagWhenPulledValueIsZero) {
    ram.write(0x0200, 0x68); // PLA
    ram.write(0x01FF, 0x00);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502StackTest, PlaSetsNegativeFlagWhenPulledValueIsNegative) {
    ram.write(0x0200, 0x68); // PLA
    ram.write(0x01FF, 0x80);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);
    cpu.NFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502StackTest, PlaFourTicksCompleteInstruction) {
    ram.write(0x0200, 0x68); // PLA
    ram.write(0x01FF, 0x42);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502StackTest, PhaThenPlaRoundTripsAccumulator) {
    ram.write(0x0200, 0x48); // PHA
    ram.write(0x0201, 0x68); // PLA
    cpu.reset();
    cpu.PC(0x0200);
    uint8_t startingSP = cpu.SP();
    cpu.A(0x37);

    cpu.executeInstruction(); // PHA
    cpu.A(0x00);
    cpu.executeInstruction(); // PLA

    EXPECT_EQ(cpu.A(), 0x37);
    EXPECT_EQ(cpu.SP(), startingSP);
}

TEST_F(CPU6502StackTest, PhpPushesStatusRegisterWithBFlagForced) {
    ram.write(0x0200, 0x08); // PHP
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.CFlag(true);
    cpu.ZFlag(false);
    cpu.NFlag(true);
    cpu.BFlag(false);

    cpu.executeInstruction();

    uint8_t pushed = ram.read(0x01FF);
    EXPECT_TRUE(pushed & 0x01); // C
    EXPECT_TRUE(pushed & 0x10); // B forced to 1
    EXPECT_TRUE(pushed & 0x80); // N
    EXPECT_EQ(cpu.SP(), 0xFE);
}

TEST_F(CPU6502StackTest, PhpDoesNotMutateLiveBFlagBeyondThePush) {
    // Forcing B into the pushed byte is a copy, not a state change -- BRK's
    // commitBRK does mutate the live BFlag as a side effect of forcing it,
    // but nothing requires PHP to follow that same path.
    ram.write(0x0200, 0x08); // PHP
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.BFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x01FF) & 0x10, 0x10);
}

TEST_F(CPU6502StackTest, PlpRestoresStatusRegisterExactly) {
    ram.write(0x0200, 0x28); // PLP
    ram.write(0x01FF, 0xC3); // 1100 0011: N,V,Z,C set; B,D,I clear
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFE);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.IFlag());
    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502StackTest, PhpThenPlpRoundTripsStatusRegister) {
    ram.write(0x0200, 0x08); // PHP
    ram.write(0x0201, 0x28); // PLP
    cpu.reset();
    cpu.PC(0x0200);
    cpu.CFlag(true);
    cpu.ZFlag(false);
    cpu.IFlag(true);
    cpu.DFlag(false);
    // PHP always forces B to 1 in the byte it pushes (no physical B
    // flip-flop -- see commitImpliedPush()), so a lossless round trip
    // requires starting with B already set; otherwise PLP would restore a
    // B bit that PHP's own push unconditionally overwrote.
    cpu.BFlag(true);
    cpu.VFlag(true);
    cpu.NFlag(false);
    uint8_t startingP = cpu.P();
    uint8_t startingSP = cpu.SP();

    cpu.executeInstruction(); // PHP
    cpu.P(0x00);
    cpu.executeInstruction(); // PLP

    EXPECT_EQ(cpu.P(), startingP);
    EXPECT_EQ(cpu.SP(), startingSP);
}
