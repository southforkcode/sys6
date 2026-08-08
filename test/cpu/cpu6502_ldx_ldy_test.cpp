#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502LdxLdyTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502LdxLdyTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502LdxLdyTest, LdxImmediateLoadsValueIntoX) {
    ram.write(0x0000, 0xA2); // LDX #imm
    ram.write(0x0001, 0x42);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdxImmediateSetsZeroFlagOnZero) {
    ram.write(0x0000, 0xA2);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.X(0xFF);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502LdxLdyTest, LdxImmediateSetsNegativeFlagOnHighBitSet) {
    ram.write(0x0000, 0xA2);
    ram.write(0x0001, 0x80);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502LdxLdyTest, TwoTicksCompleteLdxImmediate) {
    ram.write(0x0000, 0xA2);
    ram.write(0x0001, 0x42);
    cpu.reset();

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }

    EXPECT_EQ(cpu.X(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdxZeroPageLoadsValueFromMemory) {
    ram.write(0x0000, 0xA6); // LDX zp
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdxZeroPageYLoadsValueFromIndexedMemory) {
    ram.write(0x0000, 0xB6); // LDX zp,Y
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x77); // 0x10 + Y(0x05)
    cpu.reset();
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdxAbsoluteLoadsValueFromMemory) {
    ram.write(0x0000, 0xAE); // LDX abs
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // 0x0200
    ram.write(0x0200, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LdxLdyTest, LdxAbsoluteYLoadsValueFromIndexedMemory) {
    ram.write(0x0000, 0xBE); // LDX abs,Y
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200
    ram.write(0x0205, 0x77);
    cpu.reset();
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.X(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LdxLdyTest, LdxAbsoluteYPageCrossTakesExtraCycle) {
    ram.write(0x0000, 0xBE); // LDX abs,Y
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x00); // base = 0x00FF; +Y crosses into page 1
    ram.write(0x0104, 0x77);
    cpu.reset();
    cpu.Y(0x05);

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(cpu.X(), 0x00); // page crossed: not yet committed after 4 cycles

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.X(), 0x77);
}

TEST_F(CPU6502LdxLdyTest, LdyImmediateLoadsValueIntoY) {
    ram.write(0x0000, 0xA0); // LDY #imm
    ram.write(0x0001, 0x42);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdyImmediateSetsZeroFlagOnZero) {
    ram.write(0x0000, 0xA0);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.Y(0xFF);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502LdxLdyTest, LdyImmediateSetsNegativeFlagOnHighBitSet) {
    ram.write(0x0000, 0xA0);
    ram.write(0x0001, 0x80);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502LdxLdyTest, TwoTicksCompleteLdyImmediate) {
    ram.write(0x0000, 0xA0);
    ram.write(0x0001, 0x42);
    cpu.reset();

    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T0
    }
    for (int i = 0; i < 4; ++i) {
        cpu.tick(); // T1
    }

    EXPECT_EQ(cpu.Y(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdyZeroPageLoadsValueFromMemory) {
    ram.write(0x0000, 0xA4); // LDY zp
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdyZeroPageXLoadsValueFromIndexedMemory) {
    ram.write(0x0000, 0xB4); // LDY zp,X
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x77); // 0x10 + X(0x05)
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LdxLdyTest, LdyAbsoluteLoadsValueFromMemory) {
    ram.write(0x0000, 0xAC); // LDY abs
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // 0x0200
    ram.write(0x0200, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LdxLdyTest, LdyAbsoluteXLoadsValueFromIndexedMemory) {
    ram.write(0x0000, 0xBC); // LDY abs,X
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200
    ram.write(0x0205, 0x77);
    cpu.reset();
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.Y(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LdxLdyTest, LdyAbsoluteXPageCrossTakesExtraCycle) {
    ram.write(0x0000, 0xBC); // LDY abs,X
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x00); // base = 0x00FF; +X crosses into page 1
    ram.write(0x0104, 0x77);
    cpu.reset();
    cpu.X(0x05);

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(cpu.Y(), 0x00); // page crossed: not yet committed after 4 cycles

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.Y(), 0x77);
}
