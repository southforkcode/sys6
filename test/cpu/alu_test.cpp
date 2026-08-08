#include <gtest/gtest.h>

#include "cpu/alu.h"

TEST(ALUTest, AdcAddsTwoValuesWithNoCarryIn) {
    ALU alu;
    AluResult result = alu.adc(0x10, 0x05, false);

    EXPECT_EQ(result.value, 0x15);
    EXPECT_FALSE(result.carry);
    EXPECT_FALSE(result.zero);
    EXPECT_FALSE(result.overflow);
    EXPECT_FALSE(result.negative);
}

TEST(ALUTest, AdcAddsCarryInWhenSet) {
    ALU alu;
    AluResult result = alu.adc(0x10, 0x05, true);

    EXPECT_EQ(result.value, 0x16);
}

TEST(ALUTest, AdcSetsCarryAndZeroOnUnsignedOverflowToZero) {
    ALU alu;
    AluResult result = alu.adc(0xFF, 0x01, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, AdcClearsZeroFlagWhenResultIsNonZero) {
    ALU alu;
    AluResult result = alu.adc(0x01, 0x00, false);

    EXPECT_FALSE(result.zero);
}

TEST(ALUTest, AdcSetsNegativeFlagWhenBit7OfResultIsSet) {
    ALU alu;
    AluResult result = alu.adc(0x50, 0x50, false);

    EXPECT_EQ(result.value, 0xA0);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoPositivesOverflowToNegative) {
    ALU alu;
    AluResult result = alu.adc(0x7F, 0x01, false);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.overflow);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, AdcClearsOverflowWhenOperandsHaveDifferentSigns) {
    ALU alu;
    AluResult result = alu.adc(0x50, 0xFF, false);

    EXPECT_EQ(result.value, 0x4F);
    EXPECT_TRUE(result.carry);
    EXPECT_FALSE(result.overflow);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoNegativesOverflowToPositive) {
    ALU alu;
    AluResult result = alu.adc(0x80, 0x80, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.overflow);
    EXPECT_FALSE(result.negative);
}
