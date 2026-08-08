#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502LoadStoreTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502LoadStoreTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502LoadStoreTest, LdaImmediateLoadsValueIntoA) {
    ram.write(0x0000, 0xA9); // LDA #imm
    ram.write(0x0001, 0x42);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateSetsZeroFlagOnZero) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.A(0xFF);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateSetsNegativeFlagOnHighBitSet) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x80);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateDoesNotAffectCarryFlag) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502LoadStoreTest, TwoTicksCompleteLdaImmediate) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x42);
    cpu.reset();

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, LdaZeroPageLoadsValueFromMemory) {
    ram.write(0x0000, 0xA5); // LDA zp
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, ThreeTicksCompleteLdaZeroPage) {
    ram.write(0x0000, 0xA5);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }
    EXPECT_EQ(cpu.A(), 0x00); // not yet committed

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T2
    }

    EXPECT_EQ(cpu.A(), 0x77);
}

TEST_F(CPU6502LoadStoreTest, StaZeroPageWritesAccumulatorToMemory) {
    ram.write(0x0000, 0x85); // STA zp
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.A(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0020), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, StaZeroPageDoesNotAffectFlags) {
    ram.write(0x0000, 0x85);
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.A(0x00); // would set Z if STA touched flags like an ALU op
    cpu.ZFlag(false);
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502LoadStoreTest, StaAbsoluteYWritesAccumulatorToIndexedAddress) {
    ram.write(0x0000, 0x99); // STA abs,Y
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200
    cpu.reset();
    cpu.A(0x55);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0205), 0x55);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LoadStoreTest, FiveTicksCompleteStaAbsoluteYRegardlessOfPageCross) {
    ram.write(0x0000, 0x99); // STA abs,Y
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x00); // base = 0x00FF; +Y crosses into page 1
    cpu.reset();
    cpu.A(0x33);
    cpu.Y(0x01); // 0x00FF + 1 = 0x0100: page crossed

    for (int step = 0; step < 5; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(ram.read(0x0100), 0x33);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LoadStoreTest, StaAbsoluteYWithNoPageCrossStillTakesFiveTicks) {
    ram.write(0x0000, 0x99);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200, no page cross
    cpu.reset();
    cpu.A(0x11);
    cpu.Y(0x01);

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(ram.read(0x0201), 0x00); // not yet written after 4 cycles

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // 5th cycle
    }

    EXPECT_EQ(ram.read(0x0201), 0x11);
}
