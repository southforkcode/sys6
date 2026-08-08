#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502Test : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502Test() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST(CPU6502Smoke, GTestWiringWorks) { EXPECT_TRUE(true); }

TEST_F(CPU6502Test, IdReturnsMos6502) { EXPECT_EQ(cpu.id(), CpuId::Mos6502); }

TEST_F(CPU6502Test, ColdResetLoadsPCFromVectorAndLandsSPOnFD) {
    ram.write(0xFFFC, 0x34);
    ram.write(0xFFFD, 0x12);

    cpu.reset();

    // A freshly constructed CPU (not yet reset) default-initializes A/X/Y/D
    // to 0/0/0/false, so these read as "power-on defaults", not because
    // reset() force-clears them -- see WarmResetPreservesAXYAndDFlag below,
    // which proves reset() genuinely leaves them alone.
    EXPECT_EQ(cpu.A(), 0);
    EXPECT_EQ(cpu.X(), 0);
    EXPECT_EQ(cpu.Y(), 0);
    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_EQ(cpu.SP(), 0xFD); // 0x00 - 3, wrapping
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.halted());
}

TEST_F(CPU6502Test, WarmResetPreservesAXYAndDFlagAndDecrementsSPByThree) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.A(0x11);
    cpu.X(0x22);
    cpu.Y(0x33);
    cpu.DFlag(true);
    cpu.SP(0x80);

    cpu.reset();

    EXPECT_EQ(cpu.A(), 0x11);
    EXPECT_EQ(cpu.X(), 0x22);
    EXPECT_EQ(cpu.Y(), 0x33);
    EXPECT_TRUE(cpu.DFlag());
    EXPECT_EQ(cpu.SP(), 0x7D); // 0x80 - 3
}

TEST_F(CPU6502Test, ResetDecrementsSPWithPageWraparound) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.SP(0x01);

    cpu.reset();

    EXPECT_EQ(cpu.SP(), 0xFE); // 0x01 - 3, wraps within page 1
}

TEST_F(CPU6502Test, ResetDoesNotForceBFlagOrBit5) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.P(0x00); // clear everything, including B and bit 5

    cpu.reset();

    EXPECT_FALSE(cpu.BFlag());
    EXPECT_EQ(cpu.P() & 0x20, 0);
}

TEST_F(CPU6502Test, SevenTStatesCompleteResetWhenDrivenManually) {
    ram.write(0xFFFC, 0x34);
    ram.write(0xFFFD, 0x12);
    cpu.PC(0x5555);
    cpu.SP(0x80);

    cpu.beginReset();
    for (int step = 0; step < 7; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_EQ(cpu.SP(), 0x7D); // 0x80 - 3
    EXPECT_FALSE(cpu.halted());
}

TEST_F(CPU6502Test, ARoundTripsBoundaryValues) {
    cpu.A(0x00);
    EXPECT_EQ(cpu.A(), 0x00);
    cpu.A(0xFF);
    EXPECT_EQ(cpu.A(), 0xFF);
}

TEST_F(CPU6502Test, XRoundTripsBoundaryValues) {
    cpu.X(0x00);
    EXPECT_EQ(cpu.X(), 0x00);
    cpu.X(0xFF);
    EXPECT_EQ(cpu.X(), 0xFF);
}

TEST_F(CPU6502Test, YRoundTripsBoundaryValues) {
    cpu.Y(0x00);
    EXPECT_EQ(cpu.Y(), 0x00);
    cpu.Y(0xFF);
    EXPECT_EQ(cpu.Y(), 0xFF);
}

TEST_F(CPU6502Test, PCRoundTripsBoundaryValues) {
    cpu.PC(0x0000);
    EXPECT_EQ(cpu.PC(), 0x0000);
    cpu.PC(0xFFFF);
    EXPECT_EQ(cpu.PC(), 0xFFFF);
}

TEST_F(CPU6502Test, SPRoundTripsBoundaryValues) {
    cpu.SP(0x00);
    EXPECT_EQ(cpu.SP(), 0x00);
    cpu.SP(0xFF);
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502Test, EachFlagRoundTripsIndependently) {
    cpu.P(0x00);

    cpu.CFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    cpu.CFlag(false);
    EXPECT_FALSE(cpu.CFlag());

    cpu.ZFlag(true);
    EXPECT_TRUE(cpu.ZFlag());
    cpu.ZFlag(false);
    EXPECT_FALSE(cpu.ZFlag());

    cpu.IFlag(true);
    EXPECT_TRUE(cpu.IFlag());
    cpu.IFlag(false);
    EXPECT_FALSE(cpu.IFlag());

    cpu.DFlag(true);
    EXPECT_TRUE(cpu.DFlag());
    cpu.DFlag(false);
    EXPECT_FALSE(cpu.DFlag());

    cpu.BFlag(true);
    EXPECT_TRUE(cpu.BFlag());
    cpu.BFlag(false);
    EXPECT_FALSE(cpu.BFlag());

    cpu.VFlag(true);
    EXPECT_TRUE(cpu.VFlag());
    cpu.VFlag(false);
    EXPECT_FALSE(cpu.VFlag());

    cpu.NFlag(true);
    EXPECT_TRUE(cpu.NFlag());
    cpu.NFlag(false);
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502Test, SettingOneFlagDoesNotDisturbOthers) {
    cpu.P(0x00);

    cpu.CFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_FALSE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.BFlag());
    EXPECT_FALSE(cpu.VFlag());
    EXPECT_FALSE(cpu.NFlag());

    cpu.NFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_FALSE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.BFlag());
    EXPECT_FALSE(cpu.VFlag());
}

TEST_F(CPU6502Test, SettingPIsReflectedByIndividualFlags) {
    // N=1 V=1 unused=0 B=1 D=0 I=1 Z=0 C=1
    cpu.P(0b11010101);

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_TRUE(cpu.BFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502Test, SettingIndividualFlagsIsReflectedByP) {
    cpu.P(0x00);

    cpu.CFlag(true);
    cpu.IFlag(true);
    cpu.BFlag(true);
    cpu.VFlag(true);
    cpu.NFlag(true);

    EXPECT_EQ(cpu.P(), 0b11010101);
}

TEST_F(CPU6502Test, ExecuteInstructionFetchesOpcodeAndAdvancesPC) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, ExecuteInstructionReadsFromCurrentPC) {
    ram.write(0x1234, 0x42); // unimplemented opcode: treated as a 1-cycle no-op
    cpu.reset();
    cpu.PC(0x1234);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1235);
}

TEST_F(CPU6502Test, FourTicksCompleteOpcodeFetch) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, TickDoesNotAdvancePCDuringIntermediateClockPhases) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick(); // Low -> LowToHigh: settling, no commit
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // LowToHigh -> High: capture opcode into m_IR, PC not yet touched
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // High -> HighToLow: settling, no commit
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // HighToLow -> Low: commit -- PC++, CpuStep advances to T1
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, TickCompletesUnimplementedOpcodeAsOneCycleNoOp) {
    ram.write(0x0000, 0x42);
    ram.write(0x0001, 0x99);
    cpu.reset();

    cpu.tick(); // T0 phase 1/4: Low -> LowToHigh
    cpu.tick(); // T0 phase 2/4: LowToHigh -> High, capture opcode 0x42
    cpu.tick(); // T0 phase 3/4: High -> HighToLow
    cpu.tick(); // T0 phase 4/4: HighToLow -> Low, commit: PC -> 1, CpuStep -> T1

    cpu.tick(); // T1 phase 1/4
    cpu.tick(); // T1 phase 2/4: unimplemented opcode, no capture work
    cpu.tick(); // T1 phase 3/4
    cpu.tick(); // T1 phase 4/4: commit: CpuStep -> T0, instruction complete

    EXPECT_EQ(cpu.PC(), 0x0001);

    cpu.tick(); // next instruction, T0 phase 1/4
    cpu.tick(); // T0 phase 2/4: capture opcode 0x99
    cpu.tick(); // T0 phase 3/4
    cpu.tick(); // T0 phase 4/4: commit: PC -> 2

    EXPECT_EQ(cpu.PC(), 0x0002);
}
