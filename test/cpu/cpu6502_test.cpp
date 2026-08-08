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

TEST_F(CPU6502Test, ResetSetsRegistersAndFlagsToPowerOnState) {
    cpu.reset();

    EXPECT_EQ(cpu.A(), 0);
    EXPECT_EQ(cpu.X(), 0);
    EXPECT_EQ(cpu.Y(), 0);
    EXPECT_EQ(cpu.PC(), 0);
    EXPECT_EQ(cpu.SP(), 0xFF);
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_TRUE(cpu.BFlag());
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
    ram.write(0x1234, 0x99);
    cpu.reset();
    cpu.PC(0x1234);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1235);
}

TEST_F(CPU6502Test, TickPerformsOpcodeFetchOnFirstCall) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, TickCompletesUnimplementedOpcodeAsOneCycleNoOp) {
    ram.write(0x0000, 0x42);
    ram.write(0x0001, 0x99);
    cpu.reset();

    cpu.tick(); // cycle 0 of first instruction: fetch 0x42, PC -> 1
    cpu.tick(); // cycle 1 of first instruction: unimplemented, no-op, completes

    EXPECT_EQ(cpu.PC(), 0x0001);

    cpu.tick(); // cycle 0 of second instruction: fetch 0x99, PC -> 2

    EXPECT_EQ(cpu.PC(), 0x0002);
}
