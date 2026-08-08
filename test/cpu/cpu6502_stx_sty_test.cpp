#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502StxStyTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502StxStyTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502StxStyTest, StxZeroPageWritesXToMemory) {
    ram.write(0x0000, 0x86); // STX zp
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.X(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0020), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502StxStyTest, StxZeroPageDoesNotAffectFlags) {
    ram.write(0x0000, 0x86);
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.X(0x00);
    cpu.ZFlag(false);
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502StxStyTest, StxZeroPageYWritesXToIndexedMemory) {
    ram.write(0x0000, 0x96); // STX zp,Y
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.X(0x99);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0015), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502StxStyTest, StxAbsoluteWritesXToMemory) {
    ram.write(0x0000, 0x8E); // STX abs
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // 0x0200
    cpu.reset();
    cpu.X(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0200), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502StxStyTest, StyZeroPageWritesYToMemory) {
    ram.write(0x0000, 0x84); // STY zp
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.Y(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0020), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502StxStyTest, StyZeroPageDoesNotAffectFlags) {
    ram.write(0x0000, 0x84);
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.Y(0x00);
    cpu.ZFlag(false);
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502StxStyTest, StyZeroPageXWritesYToIndexedMemory) {
    ram.write(0x0000, 0x94); // STY zp,X
    ram.write(0x0001, 0x10);
    cpu.reset();
    cpu.Y(0x99);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0015), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502StxStyTest, StyAbsoluteWritesYToMemory) {
    ram.write(0x0000, 0x8C); // STY abs
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // 0x0200
    cpu.reset();
    cpu.Y(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0200), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0003);
}
