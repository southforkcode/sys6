#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502NopRtiTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502NopRtiTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502NopRtiTest, NopAdvancesProgramCounterByOne) {
    ram.write(0x0000, 0xEA); // NOP
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502NopRtiTest, NopDoesNotChangeRegistersOrFlags) {
    ram.write(0x0000, 0xEA);
    cpu.reset();
    cpu.A(0x11);
    cpu.X(0x22);
    cpu.Y(0x33);
    cpu.P(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x11);
    EXPECT_EQ(cpu.X(), 0x22);
    EXPECT_EQ(cpu.Y(), 0x33);
    EXPECT_EQ(cpu.P(), 0x00);
}

TEST_F(CPU6502NopRtiTest, TwoTicksCompleteNop) {
    ram.write(0x0000, 0xEA);
    ram.write(0x0001, 0xEA);
    cpu.reset();

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    EXPECT_EQ(cpu.PC(), 0x0001);

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }
    EXPECT_EQ(cpu.PC(), 0x0001); // still on the same instruction, no operand consumed

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0 of the next NOP
    }
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502NopRtiTest, RtiPullsStatusRegisterFromStack) {
    ram.write(0x0200, 0x40); // RTI
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFC);
    ram.write(0x01FD, 0xA5); // pulled P
    ram.write(0x01FE, 0x00); // PCL
    ram.write(0x01FF, 0x10); // PCH

    cpu.executeInstruction();

    EXPECT_EQ(cpu.P(), 0xA5);
}

TEST_F(CPU6502NopRtiTest, RtiPullsProgramCounterFromStack) {
    ram.write(0x0200, 0x40); // RTI
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFC);
    ram.write(0x01FD, 0x00);
    ram.write(0x01FE, 0x34);
    ram.write(0x01FF, 0x12);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502NopRtiTest, RtiIncrementsStackPointerByThree) {
    ram.write(0x0200, 0x40);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFC);
    ram.write(0x01FD, 0x00);
    ram.write(0x01FE, 0x00);
    ram.write(0x01FF, 0x10);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502NopRtiTest, SixTicksCompleteRti) {
    ram.write(0x0200, 0x40);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFC);
    ram.write(0x01FD, 0x00);
    ram.write(0x01FE, 0x34);
    ram.write(0x01FF, 0x12);

    for (int step = 0; step < 5; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(cpu.PC(), 0x0201); // not yet restored

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502NopRtiTest, BrkThenRtiRoundTripsBackToInterruptedProgram) {
    ram.write(0x0300, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90); // BRK vector -> 0x9000
    ram.write(0x9000, 0x40); // RTI at the "interrupt handler"
    cpu.reset();
    cpu.PC(0x0300);
    cpu.SP(0xFF);
    cpu.P(0x20); // only the always-set bit 5

    cpu.executeInstruction(); // BRK: pushes PC=0x0302, P=0x30, jumps to 0x9000
    EXPECT_EQ(cpu.PC(), 0x9000);

    cpu.executeInstruction(); // RTI: should return to 0x0302 with P restored

    EXPECT_EQ(cpu.PC(), 0x0302);
    EXPECT_EQ(cpu.P(), 0x30);
    EXPECT_EQ(cpu.SP(), 0xFF);
}
