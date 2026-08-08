#include "cpu6502.h"

#include "memory/bus.h"

#include <sstream>
#include <stdbool.h>
#include <stdint.h>

const uint16_t cNMIVector = 0xfffa;
const uint16_t cResetVector = 0xfffc;
const uint16_t cBRKVector = 0xfffe;

const uint8_t cOpADCImmediate = 0x69;
const uint8_t cOpADCAbsolute = 0x6D;
const uint8_t cOpADCZeroPage = 0x65;
const uint8_t cOpADCZeroPageX = 0x75;
const uint8_t cOpADCAbsoluteX = 0x7D;
const uint8_t cOpADCAbsoluteY = 0x79;

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
    // m_cpuStep == T0 is ambiguous on its own: it's true both before the
    // instruction starts and once it completes. m_clockPhase == Low
    // disambiguates -- that combination only recurs once the final commit
    // of the instruction's last T-state has actually run.
    do {
        tick();
    } while (m_cpuStep != CpuStep::T0 || m_clockPhase != ClockPhase::Low);
}

namespace {
ClockPhase nextClockPhase(ClockPhase phase) {
    switch (phase) {
    case ClockPhase::Low:
        return ClockPhase::LowToHigh;
    case ClockPhase::LowToHigh:
        return ClockPhase::High;
    case ClockPhase::High:
        return ClockPhase::HighToLow;
    case ClockPhase::HighToLow:
        return ClockPhase::Low;
    }
    return ClockPhase::Low; // unreachable: all enumerators handled above
}
} // namespace

void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle.
    m_aluOutput = m_alu.adc(m_aluA, m_aluB, m_aluCarryIn);

    m_clockPhase = nextClockPhase(m_clockPhase);

    switch (m_clockPhase) {
    case ClockPhase::High:
        onClockHigh();
        break;
    case ClockPhase::Low:
        onClockLow();
        break;
    default:
        break; // LowToHigh / HighToLow: settling only, no commits.
    }
}

void CPU6502::runToClockHigh() {
    do {
        tick();
    } while (m_clockPhase != ClockPhase::High);
}

void CPU6502::onClockHigh() {
    if (m_cpuStep == CpuStep::T0) {
        captureOpcodeFetch();
        return;
    }

    switch (m_IR) {
    case cOpADCImmediate:
        captureADCImmediate();
        break;
    case cOpADCAbsolute:
        captureADCAbsolute();
        break;
    case cOpADCZeroPage:
        captureADCZeroPage();
        break;
    case cOpADCZeroPageX:
        captureADCZeroPageX();
        break;
    case cOpADCAbsoluteX:
        captureADCAbsoluteX();
        break;
    case cOpADCAbsoluteY:
        captureADCAbsoluteY();
        break;
    default:
        break; // unimplemented opcode: nothing to capture
    }
}

void CPU6502::onClockLow() {
    if (m_cpuStep == CpuStep::T0) {
        commitOpcodeFetch();
        return;
    }

    switch (m_IR) {
    case cOpADCImmediate:
        commitADCImmediate();
        break;
    case cOpADCAbsolute:
        commitADCAbsolute();
        break;
    case cOpADCZeroPage:
        commitADCZeroPage();
        break;
    case cOpADCZeroPageX:
        commitADCZeroPageX();
        break;
    case cOpADCAbsoluteX:
        commitADCAbsoluteX();
        break;
    case cOpADCAbsoluteY:
        commitADCAbsoluteY();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}

EffectiveAddress CPU6502::indexedAddress(uint16_t base, uint8_t index) {
    auto address = static_cast<uint16_t>(base + index);
    bool pageCrossed = (base & 0xFF00) != (address & 0xFF00);
    return EffectiveAddress{address, pageCrossed};
}

void CPU6502::captureOpcodeFetch() { m_IR = m_bus.read(m_PC); }

void CPU6502::commitOpcodeFetch() {
    m_PC++;

    if (m_tracing && m_logger) {
        std::ostringstream oss;
        oss << "Fetched opcode 0x" << std::hex << std::uppercase << static_cast<int>(m_IR)
            << " at PC 0x" << (m_PC - 1);
        m_logger->trace(oss.str());
    }

    m_cpuStep = CpuStep::T1;
}

void CPU6502::loadAluInputs(uint8_t operand) {
    m_aluA = m_A;
    m_aluB = operand;
    m_aluCarryIn = CFlag();
}

void CPU6502::commitAluResult() {
    A(m_aluOutput.value);
    CFlag(m_aluOutput.carry);
    ZFlag(m_aluOutput.zero);
    VFlag(m_aluOutput.overflow);
    NFlag(m_aluOutput.negative);
}

void CPU6502::captureADCImmediate() { loadAluInputs(m_bus.read(m_PC)); }

void CPU6502::commitADCImmediate() {
    m_PC++;
    commitAluResult();
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureADCAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(m_PC)) << 8;
        break;
    case CpuStep::T3:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}

void CPU6502::commitADCAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}

void CPU6502::captureADCZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitADCZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureADCZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}

void CPU6502::commitADCZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}

void CPU6502::captureADCAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base = static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_X);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            loadAluInputs(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitADCAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T4;
        } else {
            commitAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::captureADCAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base = static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            loadAluInputs(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitADCAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T4;
        } else {
            commitAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}
