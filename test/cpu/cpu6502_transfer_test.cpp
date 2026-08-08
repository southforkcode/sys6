#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502TransferTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502TransferTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502TransferTest, TaxCopiesAccumulatorIntoX) {
    ram.write(0x0000, 0xAA); // TAX
    cpu.reset();
    cpu.A(0x42);
    cpu.X(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x42);
    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502TransferTest, TaxSetsZeroFlagWhenAccumulatorIsZero) {
    ram.write(0x0000, 0xAA);
    cpu.reset();
    cpu.A(0x00);
    cpu.X(0xFF);
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x00);
    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502TransferTest, TaxSetsNegativeFlagWhenHighBitSet) {
    ram.write(0x0000, 0xAA);
    cpu.reset();
    cpu.A(0x80);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502TransferTest, EightTicksCompleteTax) {
    ram.write(0x0000, 0xAA);
    cpu.reset();
    cpu.A(0x42);

    for (int i = 0; i < 7; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.X(), 0x00);

    cpu.tick();
    EXPECT_EQ(cpu.X(), 0x42);
}

TEST_F(CPU6502TransferTest, TxaCopiesXIntoAccumulator) {
    ram.write(0x0000, 0x8A); // TXA
    cpu.reset();
    cpu.X(0x37);
    cpu.A(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x37);
    EXPECT_EQ(cpu.X(), 0x37);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502TransferTest, TxaSetsZeroAndNegativeFlags) {
    ram.write(0x0000, 0x8A);
    cpu.reset();
    cpu.X(0x00);
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502TransferTest, TayCopiesAccumulatorIntoY) {
    ram.write(0x0000, 0xA8); // TAY
    cpu.reset();
    cpu.A(0x55);
    cpu.Y(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x55);
    EXPECT_EQ(cpu.A(), 0x55);
}

TEST_F(CPU6502TransferTest, TaySetsNegativeFlagWhenHighBitSet) {
    ram.write(0x0000, 0xA8);
    cpu.reset();
    cpu.A(0x90);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502TransferTest, TyaCopiesYIntoAccumulator) {
    ram.write(0x0000, 0x98); // TYA
    cpu.reset();
    cpu.Y(0x64);
    cpu.A(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x64);
    EXPECT_EQ(cpu.Y(), 0x64);
}

TEST_F(CPU6502TransferTest, TyaSetsZeroFlagWhenYIsZero) {
    ram.write(0x0000, 0x98);
    cpu.reset();
    cpu.Y(0x00);
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502TransferTest, TsxCopiesStackPointerIntoX) {
    ram.write(0x0000, 0xBA); // TSX
    cpu.reset();
    cpu.SP(0x80);
    cpu.X(0x00);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x80);
    EXPECT_EQ(cpu.SP(), 0x80);
}

TEST_F(CPU6502TransferTest, TsxSetsNegativeFlagWhenHighBitSet) {
    ram.write(0x0000, 0xBA);
    cpu.reset();
    cpu.SP(0xC0);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502TransferTest, TxsCopiesXIntoStackPointer) {
    ram.write(0x0000, 0x9A); // TXS
    cpu.reset();
    cpu.X(0x20);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.SP(), 0x20);
    EXPECT_EQ(cpu.X(), 0x20);
    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502TransferTest, TxsDoesNotAffectAnyFlags) {
    ram.write(0x0000, 0x9A);
    cpu.reset();
    cpu.X(0x00); // would set Z if TXS touched flags like every other transfer
    cpu.ZFlag(false);
    cpu.NFlag(true);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_TRUE(cpu.CFlag());
}
