#include "cpu6502.h"

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
    uint8_t ra = m_A;
    uint8_t rx = m_X;
    uint8_t ry = m_Y;
    uint16_t rpc = m_PC;
    uint16_t rsp = 0x100 + m_SP;
}
