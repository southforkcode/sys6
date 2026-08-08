#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502ClockTestAccess : public CPU6502 {
public:
    using CPU6502::CPU6502;
    using CPU6502::m_aluA;
    using CPU6502::m_aluB;
    using CPU6502::m_aluCarryIn;
    using CPU6502::m_aluOutput;
};

class CPU6502ClockTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502ClockTestAccess cpu{bus};

    CPU6502ClockTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502ClockTest, AluRecomputesOnTickRegardlessOfOpcode) {
    ram.write(0x0000, 0x42); // unimplemented opcode: no opcode-specific ALU use
    cpu.reset();
    cpu.m_aluA = 0x01;
    cpu.m_aluB = 0x01;
    cpu.m_aluCarryIn = false;

    cpu.tick();

    EXPECT_EQ(cpu.m_aluOutput.value, 0x02);
}

TEST_F(CPU6502ClockTest, AluOutputTracksLatestInputsAcrossTicks) {
    ram.write(0x0000, 0x42);
    cpu.reset();
    cpu.m_aluA = 0x01;
    cpu.m_aluB = 0x01;
    cpu.m_aluCarryIn = false;

    cpu.tick();
    EXPECT_EQ(cpu.m_aluOutput.value, 0x02);

    cpu.m_aluB = 0x05;
    cpu.tick();

    EXPECT_EQ(cpu.m_aluOutput.value, 0x06);
}

TEST_F(CPU6502ClockTest, RunToClockHighAdvancesExactlyToNextHighPhase) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.runToClockHigh(); // Low -> LowToHigh -> High: opcode captured, PC untouched

    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // High -> HighToLow
    cpu.tick(); // HighToLow -> Low: commit, PC -> 1

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502ClockTest, RunToClockHighSupportsSteppingWholeInstructionsByCycle) {
    ram.write(0x0000, 0x69); // ADC #immediate
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.runToClockHigh(); // T0: opcode captured
    cpu.tick();
    cpu.tick(); // T0 commit: PC -> 1, CpuStep -> T1

    cpu.runToClockHigh(); // T1: operand + ALU inputs captured, A not yet updated
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick(); // T1 commit: A -> 0x15

    EXPECT_EQ(cpu.A(), 0x15);
}
