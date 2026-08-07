#include <gtest/gtest.h>

#include "cpu/cpu6502.h"

TEST(CPU6502Smoke, GTestWiringWorks) {
    EXPECT_TRUE(true);
}

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
