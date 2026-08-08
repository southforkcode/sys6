#include <gtest/gtest.h>

#include "cpu/alu.h"

TEST(ALUTest, AdcAddsTwoValuesWithNoCarryIn) {
    ALU alu;
    AluResult r = alu.adc(0x10, 0x05, false);

    EXPECT_EQ(r.value, 0x15);
    EXPECT_FALSE(r.carry);
    EXPECT_FALSE(r.zero);
    EXPECT_FALSE(r.overflow);
    EXPECT_FALSE(r.negative);
}

TEST(ALUTest, AdcAddsCarryInWhenSet) {
    ALU alu;
    AluResult r = alu.adc(0x10, 0x05, true);

    EXPECT_EQ(r.value, 0x16);
}

TEST(ALUTest, AdcSetsCarryAndZeroOnUnsignedOverflowToZero) {
    ALU alu;
    AluResult r = alu.adc(0xFF, 0x01, false);

    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.carry);
    EXPECT_TRUE(r.zero);
}

TEST(ALUTest, AdcClearsZeroFlagWhenResultIsNonZero) {
    ALU alu;
    AluResult r = alu.adc(0x01, 0x00, false);

    EXPECT_FALSE(r.zero);
}

TEST(ALUTest, AdcSetsNegativeFlagWhenBit7OfResultIsSet) {
    ALU alu;
    AluResult r = alu.adc(0x50, 0x50, false);

    EXPECT_EQ(r.value, 0xA0);
    EXPECT_TRUE(r.negative);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoPositivesOverflowToNegative) {
    ALU alu;
    AluResult r = alu.adc(0x7F, 0x01, false);

    EXPECT_EQ(r.value, 0x80);
    EXPECT_TRUE(r.overflow);
    EXPECT_TRUE(r.negative);
}

TEST(ALUTest, AdcClearsOverflowWhenOperandsHaveDifferentSigns) {
    ALU alu;
    AluResult r = alu.adc(0x50, 0xFF, false);

    EXPECT_EQ(r.value, 0x4F);
    EXPECT_TRUE(r.carry);
    EXPECT_FALSE(r.overflow);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoNegativesOverflowToPositive) {
    ALU alu;
    AluResult r = alu.adc(0x80, 0x80, false);

    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.carry);
    EXPECT_TRUE(r.overflow);
    EXPECT_FALSE(r.negative);
}
