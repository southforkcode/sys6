#include <gtest/gtest.h>

#include "cpu/alu.h"

TEST(ALUTest, ExecuteAddAddsTwoValuesWithNoCarryIn) {
    ALU alu;
    AluResult result = alu.execute(0x10, 0x05, AluFunction::ADD, false);

    EXPECT_EQ(result.value, 0x15);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteAddIncludesCarryInWhenSet) {
    ALU alu;
    AluResult result = alu.execute(0x10, 0x05, AluFunction::ADD, true);

    EXPECT_EQ(result.value, 0x16);
}

TEST(ALUTest, ExecuteAddSetsCarryOnUnsignedOverflowToZero) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x01, AluFunction::ADD, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteAndCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xF0, 0x3C, AluFunction::AND, false);

    EXPECT_EQ(result.value, 0x30);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteOrCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xF0, 0x0C, AluFunction::OR, false);

    EXPECT_EQ(result.value, 0xFC);
}

TEST(ALUTest, ExecuteXorCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x0F, AluFunction::XOR, false);

    EXPECT_EQ(result.value, 0xF0);
}

TEST(ALUTest, ExecuteShlShiftsLeftAndCarriesOutBit7OfB) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x81, AluFunction::SHL, false);

    EXPECT_EQ(result.value, 0x02);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteShlIgnoresA) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x40, AluFunction::SHL, false);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteShrShiftsRightAndCarriesOutBit0OfB) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::SHR, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRolBringsInCarryAtBit0AndCarriesOutBit7) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x80, AluFunction::ROL, true);

    EXPECT_EQ(result.value, 0x01);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRolClearsBit0WhenCarryInFalse) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::ROL, false);

    EXPECT_EQ(result.value, 0x02);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteRorBringsInCarryAtBit7AndCarriesOutBit0) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::ROR, true);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRorClearsBit7WhenCarryInFalse) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x02, AluFunction::ROR, false);

    EXPECT_EQ(result.value, 0x01);
    EXPECT_FALSE(result.carry);
}
