#include "cpu6502.h"

#include "memory/bus.h"

#include <sstream>
#include <stdbool.h>
#include <stdint.h>

const uint16_t cNMIVector = 0xfffa;
const uint16_t cResetVector = 0xfffc;
const uint16_t cBRKVector = 0xfffe;

const auto cCFlagOffset = 0;
const auto cZFlagOffset = 1;
const auto cIFlagOffset = 2;
const auto cDFlagOffset = 3;
const auto cBFlagOffset = 4;
const auto cVFlagOffset = 6;
const auto cNFlagOffset = 7;

constexpr unsigned to_mask(unsigned offset) { return 1 << offset; }

CPU6502::CPU6502(Bus &bus) : m_bus(bus) {}

uint8_t CPU6502::A() const { return m_A; }

uint8_t CPU6502::X() const { return m_X; }

uint8_t CPU6502::Y() const { return m_Y; }

uint16_t CPU6502::PC() const { return m_PC; }

uint16_t CPU6502::SP() const { return m_SP; }

void CPU6502::A(uint8_t val) { m_A = val; }

void CPU6502::X(uint8_t val) { m_X = val; }

void CPU6502::Y(uint8_t val) { m_Y = val; }

void CPU6502::PC(uint16_t val) { m_PC = val; }

void CPU6502::SP(uint8_t val) { m_SP = val; }

uint8_t CPU6502::P() const { return m_pFlags.to_ulong() & 0xff; }

void CPU6502::P(uint8_t val) { m_pFlags = val; }

bool CPU6502::CFlag() const { return m_pFlags.test(cCFlagOffset); }

void CPU6502::CFlag(bool val) { m_pFlags.set(cCFlagOffset, val); }

bool CPU6502::ZFlag() const { return m_pFlags.test(cZFlagOffset); }

void CPU6502::ZFlag(bool val) { m_pFlags.set(cZFlagOffset, val); }

bool CPU6502::IFlag() const { return m_pFlags.test(cIFlagOffset); }

void CPU6502::IFlag(bool val) { m_pFlags.set(cIFlagOffset, val); }

bool CPU6502::DFlag() const { return m_pFlags.test(cDFlagOffset); }

void CPU6502::DFlag(bool val) { m_pFlags.set(cDFlagOffset, val); }

bool CPU6502::BFlag() const { return m_pFlags.test(cBFlagOffset); }

void CPU6502::BFlag(bool val) { m_pFlags.set(cBFlagOffset, val); }

bool CPU6502::VFlag() const { return m_pFlags.test(cVFlagOffset); }

void CPU6502::VFlag(bool val) { m_pFlags.set(cVFlagOffset, val); }

bool CPU6502::NFlag() const { return m_pFlags.test(cNFlagOffset); }

void CPU6502::NFlag(bool val) { m_pFlags.set(cNFlagOffset, val); }

void CPU6502::reset() {
    A(0);
    X(0);
    Y(0);
    PC(0);
    SP(0xff);
    IFlag(true);
    DFlag(false);
    BFlag(true);
    m_pFlags.set(5, true);
}

void CPU6502::executeInstruction() {
    do {
        tick();
    } while (m_cycle != 0);
}

void CPU6502::tick() {
    if (m_cycle == 0) {
        uint8_t opcode = m_bus.read(m_PC);
        m_IR = opcode;
        m_PC++;

        if (m_tracing && m_logger) {
            std::ostringstream oss;
            oss << "Fetched opcode 0x" << std::hex << std::uppercase << static_cast<int>(opcode)
                << " at PC 0x" << (m_PC - 1);
            m_logger->trace(oss.str());
        }

        m_cycle = 1;
        return;
    }

    switch (m_IR) {
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cycle = 0;
        break;
    }
}
