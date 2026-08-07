#include <gtest/gtest.h>

#include "cpu/cpu6502.h"

TEST(CPU6502Smoke, GTestWiringWorks) { EXPECT_TRUE(true); }

TEST(CPU6502Reset, SetsRegistersAndFlagsToPowerOnState) {
    CPU6502 cpu;
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

TEST(CPU6502Registers, ARoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.A(0x00);
    EXPECT_EQ(cpu.A(), 0x00);
    cpu.A(0xFF);
    EXPECT_EQ(cpu.A(), 0xFF);
}

TEST(CPU6502Registers, XRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.X(0x00);
    EXPECT_EQ(cpu.X(), 0x00);
    cpu.X(0xFF);
    EXPECT_EQ(cpu.X(), 0xFF);
}

TEST(CPU6502Registers, YRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.Y(0x00);
    EXPECT_EQ(cpu.Y(), 0x00);
    cpu.Y(0xFF);
    EXPECT_EQ(cpu.Y(), 0xFF);
}

TEST(CPU6502Registers, PCRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.PC(0x0000);
    EXPECT_EQ(cpu.PC(), 0x0000);
    cpu.PC(0xFFFF);
    EXPECT_EQ(cpu.PC(), 0xFFFF);
}

TEST(CPU6502Registers, SPRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.SP(0x00);
    EXPECT_EQ(cpu.SP(), 0x00);
    cpu.SP(0xFF);
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST(CPU6502Flags, EachFlagRoundTripsIndependently) {
    CPU6502 cpu;
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

TEST(CPU6502Flags, SettingOneFlagDoesNotDisturbOthers) {
    CPU6502 cpu;
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

TEST(CPU6502StatusByte, SettingPIsReflectedByIndividualFlags) {
    CPU6502 cpu;

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

TEST(CPU6502StatusByte, SettingIndividualFlagsIsReflectedByP) {
    CPU6502 cpu;
    cpu.P(0x00);

    cpu.CFlag(true);
    cpu.IFlag(true);
    cpu.BFlag(true);
    cpu.VFlag(true);
    cpu.NFlag(true);

    EXPECT_EQ(cpu.P(), 0b11010101);
}
