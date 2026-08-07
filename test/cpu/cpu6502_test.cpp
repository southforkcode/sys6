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
