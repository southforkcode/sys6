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

TEST(ALUTest, SbcSubtractsOperandWhenCarryInSet) {
    ALU alu;
    AluResult result = alu.sbc(0x10, 0x05, true);

    EXPECT_EQ(result.value, 0x0B);
    EXPECT_TRUE(result.carry);
    EXPECT_FALSE(result.zero);
}

TEST(ALUTest, SbcBorrowsWhenCarryInClear) {
    ALU alu;
    AluResult result = alu.sbc(0x10, 0x05, false);

    EXPECT_EQ(result.value, 0x0A);
}

TEST(ALUTest, SbcClearsCarryOnUnsignedUnderflow) {
    ALU alu;
    AluResult result = alu.sbc(0x00, 0x01, true);

    EXPECT_EQ(result.value, 0xFF);
    EXPECT_FALSE(result.carry);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, SbcSetsOverflowOnSignedUnderflow) {
    ALU alu;
    AluResult result = alu.sbc(0x80, 0x01, true);

    EXPECT_EQ(result.value, 0x7F);
    EXPECT_TRUE(result.overflow);
    EXPECT_FALSE(result.negative);
}

TEST(ALUTest, BitwiseAndCombinesOperandsAndIgnoresCarry) {
    ALU alu;
    AluResult result = alu.bitwiseAnd(0xF0, 0x3C);

    EXPECT_EQ(result.value, 0x30);
    EXPECT_FALSE(result.zero);
    EXPECT_FALSE(result.negative);
}

TEST(ALUTest, BitwiseAndSetsZeroWhenNoBitsOverlap) {
    ALU alu;
    AluResult result = alu.bitwiseAnd(0x0F, 0xF0);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, BitwiseAndSetsNegativeWhenBit7Survives) {
    ALU alu;
    AluResult result = alu.bitwiseAnd(0xFF, 0x80);

    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, BitwiseOrCombinesOperands) {
    ALU alu;
    AluResult result = alu.bitwiseOr(0xF0, 0x0C);

    EXPECT_EQ(result.value, 0xFC);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, BitwiseOrSetsZeroWhenBothOperandsAreZero) {
    ALU alu;
    AluResult result = alu.bitwiseOr(0x00, 0x00);

    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, BitwiseXorCombinesOperands) {
    ALU alu;
    AluResult result = alu.bitwiseXor(0xFF, 0x0F);

    EXPECT_EQ(result.value, 0xF0);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, BitwiseXorSetsZeroWhenOperandsMatch) {
    ALU alu;
    AluResult result = alu.bitwiseXor(0x5A, 0x5A);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, CmpSetsCarryWhenRegGreaterThanOperand) {
    ALU alu;
    AluResult result = alu.cmp(0x10, 0x05);

    EXPECT_TRUE(result.carry);
    EXPECT_FALSE(result.zero);
}

TEST(ALUTest, CmpSetsZeroAndCarryWhenEqual) {
    ALU alu;
    AluResult result = alu.cmp(0x10, 0x10);

    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, CmpClearsCarryWhenRegLessThanOperand) {
    ALU alu;
    AluResult result = alu.cmp(0x05, 0x10);

    EXPECT_FALSE(result.carry);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, AslShiftsLeftAndCarriesOutBit7) {
    ALU alu;
    AluResult result = alu.asl(0x81);

    EXPECT_EQ(result.value, 0x02);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, AslSetsZeroWhenResultIsZero) {
    ALU alu;
    AluResult result = alu.asl(0x00);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.zero);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, AslSetsNegativeWhenBit7OfResultIsSet) {
    ALU alu;
    AluResult result = alu.asl(0x40);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.negative);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, RolShiftsLeftAndBringsInCarry) {
    ALU alu;
    AluResult result = alu.rol(0x01, true);

    EXPECT_EQ(result.value, 0x03);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, RolCarriesOutBit7) {
    ALU alu;
    AluResult result = alu.rol(0x80, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, LsrShiftsRightAndCarriesOutBit0) {
    ALU alu;
    AluResult result = alu.lsr(0x01);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, LsrNeverSetsNegative) {
    ALU alu;
    AluResult result = alu.lsr(0xFF);

    EXPECT_EQ(result.value, 0x7F);
    EXPECT_FALSE(result.negative);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, RorShiftsRightAndBringsInCarryAtBit7) {
    ALU alu;
    AluResult result = alu.ror(0x01, true);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.carry);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, RorCarriesOutBit0) {
    ALU alu;
    AluResult result = alu.ror(0x02, false);

    EXPECT_EQ(result.value, 0x01);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, IncrementAddsOne) {
    ALU alu;
    AluResult result = alu.increment(0x05);

    EXPECT_EQ(result.value, 0x06);
    EXPECT_FALSE(result.zero);
    EXPECT_FALSE(result.negative);
}

TEST(ALUTest, IncrementWrapsToZeroAndSetsZeroFlag) {
    ALU alu;
    AluResult result = alu.increment(0xFF);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.zero);
}

TEST(ALUTest, IncrementSetsNegativeOnSignedOverflow) {
    ALU alu;
    AluResult result = alu.increment(0x7F);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, DecrementSubtractsOne) {
    ALU alu;
    AluResult result = alu.decrement(0x05);

    EXPECT_EQ(result.value, 0x04);
    EXPECT_FALSE(result.zero);
}

TEST(ALUTest, DecrementWrapsBelowZero) {
    ALU alu;
    AluResult result = alu.decrement(0x00);

    EXPECT_EQ(result.value, 0xFF);
    EXPECT_TRUE(result.negative);
}

TEST(ALUTest, DecrementSetsZeroFlag) {
    ALU alu;
    AluResult result = alu.decrement(0x01);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.zero);
}
