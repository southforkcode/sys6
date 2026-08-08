#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502JmpTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502JmpTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502JmpTest, JmpAbsoluteSetsProgramCounter) {
    ram.write(0x0000, 0x4C); // JMP abs
    ram.write(0x0001, 0x34);
    ram.write(0x0002, 0x12); // target 0x1234
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502JmpTest, ThreeTicksCompleteJmpAbsolute) {
    ram.write(0x0000, 0x4C);
    ram.write(0x0001, 0x34);
    ram.write(0x0002, 0x12);
    cpu.reset();

    for (int step = 0; step < 2; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(cpu.PC(), 0x0002); // not yet jumped

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502JmpTest, JmpIndirectSetsProgramCounterThroughPointer) {
    ram.write(0x0000, 0x6C); // JMP (ind)
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // pointer address 0x0200
    ram.write(0x0200, 0x34); // target low
    ram.write(0x0201, 0x12); // target high
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502JmpTest, JmpIndirectWrapsHighByteFetchWithinPageOnBoundary) {
    ram.write(0x0000, 0x6C); // JMP (ind)
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02); // pointer address 0x02FF: low byte at a page boundary
    ram.write(0x02FF, 0x34); // target low, read from 0x02FF
    ram.write(0x0300, 0x99); // NOT read: real hardware bug wraps instead of crossing here
    ram.write(0x0200, 0x12); // target high, read from 0x0200 (wrapped), not 0x0300
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502JmpTest, FiveTicksCompleteJmpIndirect) {
    ram.write(0x0000, 0x6C);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x34);
    ram.write(0x0201, 0x12);
    cpu.reset();

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }
    EXPECT_EQ(cpu.PC(), 0x0003); // not yet jumped

    for (int i = 0; i < 4; ++i) {
        cpu.tick();
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
}
