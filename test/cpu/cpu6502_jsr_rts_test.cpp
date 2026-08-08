#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502JsrRtsTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502JsrRtsTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502JsrRtsTest, JsrJumpsToTargetAddress) {
    ram.write(0x0200, 0x20); // JSR
    ram.write(0x0201, 0x00);
    ram.write(0x0202, 0x03);
    cpu.reset();
    cpu.PC(0x0200);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0300);
}

TEST_F(CPU6502JsrRtsTest, JsrPushesReturnAddressMinusOneOntoStack) {
    ram.write(0x0200, 0x20); // JSR
    ram.write(0x0201, 0x00);
    ram.write(0x0202, 0x03);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);

    cpu.executeInstruction();

    // JSR pushes the address of its own last byte (0x0202), not the address
    // of the next instruction (0x0203).
    EXPECT_EQ(ram.read(0x01FF), 0x02); // PCH
    EXPECT_EQ(ram.read(0x01FE), 0x02); // PCL
    EXPECT_EQ(cpu.SP(), 0xFD);
}

TEST_F(CPU6502JsrRtsTest, JsrSixTicksCompleteInstruction) {
    ram.write(0x0200, 0x20); // JSR
    ram.write(0x0201, 0x00);
    ram.write(0x0202, 0x03);
    cpu.reset();
    cpu.PC(0x0200);

    for (int step = 0; step < 6; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.PC(), 0x0300);
}

TEST_F(CPU6502JsrRtsTest, RtsPullsReturnAddressAndAddsOne) {
    ram.write(0x0300, 0x60); // RTS
    ram.write(0x01FE, 0x02); // PCL pushed by a prior JSR
    ram.write(0x01FF, 0x02); // PCH pushed by a prior JSR
    cpu.reset();
    cpu.PC(0x0300);
    cpu.SP(0xFD);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0203);
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502JsrRtsTest, RtsSixTicksCompleteInstruction) {
    ram.write(0x0300, 0x60); // RTS
    ram.write(0x01FE, 0x02);
    ram.write(0x01FF, 0x02);
    cpu.reset();
    cpu.PC(0x0300);
    cpu.SP(0xFD);

    for (int step = 0; step < 6; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.PC(), 0x0203);
    EXPECT_EQ(cpu.SP(), 0xFF);
}

TEST_F(CPU6502JsrRtsTest, JsrThenRtsRoundTripsBackToCallSite) {
    ram.write(0x0200, 0x20); // JSR 0x0300
    ram.write(0x0201, 0x00);
    ram.write(0x0202, 0x03);
    ram.write(0x0300, 0x60); // RTS
    cpu.reset();
    cpu.PC(0x0200);
    uint8_t startingSP = cpu.SP();

    cpu.executeInstruction(); // JSR
    EXPECT_EQ(cpu.PC(), 0x0300);

    cpu.executeInstruction(); // RTS
    EXPECT_EQ(cpu.PC(), 0x0203);
    EXPECT_EQ(cpu.SP(), startingSP);
}
