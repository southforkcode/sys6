#include "cpu6502.h"

#include "memory/bus.h"

#include <sstream>
#include <stdbool.h>
#include <stdint.h>

const uint16_t cNMIVector = 0xfffa;
const uint16_t cResetVector = 0xfffc;
const uint16_t cBRKVector = 0xfffe;

const uint8_t cOpBRK = 0x00;
const uint8_t cOpJSR = 0x20;
const uint8_t cOpRTS = 0x60;

const uint8_t cOpPHP = 0x08;
const uint8_t cOpPHA = 0x48;
const uint8_t cOpPLP = 0x28;
const uint8_t cOpPLA = 0x68;

const uint8_t cOpADCImmediate = 0x69;
const uint8_t cOpADCAbsolute = 0x6D;
const uint8_t cOpADCZeroPage = 0x65;
const uint8_t cOpADCZeroPageX = 0x75;
const uint8_t cOpADCAbsoluteX = 0x7D;
const uint8_t cOpADCAbsoluteY = 0x79;
const uint8_t cOpADCIndirectX = 0x61;
const uint8_t cOpADCIndirectY = 0x71;

const uint8_t cOpSBCImmediate = 0xE9;
const uint8_t cOpSBCZeroPage = 0xE5;
const uint8_t cOpSBCZeroPageX = 0xF5;
const uint8_t cOpSBCAbsolute = 0xED;
const uint8_t cOpSBCAbsoluteX = 0xFD;
const uint8_t cOpSBCAbsoluteY = 0xF9;
const uint8_t cOpSBCIndirectX = 0xE1;
const uint8_t cOpSBCIndirectY = 0xF1;

const uint8_t cOpANDImmediate = 0x29;
const uint8_t cOpANDZeroPage = 0x25;
const uint8_t cOpANDZeroPageX = 0x35;
const uint8_t cOpANDAbsolute = 0x2D;
const uint8_t cOpANDAbsoluteX = 0x3D;
const uint8_t cOpANDAbsoluteY = 0x39;
const uint8_t cOpANDIndirectX = 0x21;
const uint8_t cOpANDIndirectY = 0x31;

const uint8_t cOpORAImmediate = 0x09;
const uint8_t cOpORAZeroPage = 0x05;
const uint8_t cOpORAZeroPageX = 0x15;
const uint8_t cOpORAAbsolute = 0x0D;
const uint8_t cOpORAAbsoluteX = 0x1D;
const uint8_t cOpORAAbsoluteY = 0x19;
const uint8_t cOpORAIndirectX = 0x01;
const uint8_t cOpORAIndirectY = 0x11;

const uint8_t cOpEORImmediate = 0x49;
const uint8_t cOpEORZeroPage = 0x45;
const uint8_t cOpEORZeroPageX = 0x55;
const uint8_t cOpEORAbsolute = 0x4D;
const uint8_t cOpEORAbsoluteX = 0x5D;
const uint8_t cOpEORAbsoluteY = 0x59;
const uint8_t cOpEORIndirectX = 0x41;
const uint8_t cOpEORIndirectY = 0x51;

const uint8_t cOpCMPImmediate = 0xC9;
const uint8_t cOpCMPZeroPage = 0xC5;
const uint8_t cOpCMPZeroPageX = 0xD5;
const uint8_t cOpCMPAbsolute = 0xCD;
const uint8_t cOpCMPAbsoluteX = 0xDD;
const uint8_t cOpCMPAbsoluteY = 0xD9;
const uint8_t cOpCMPIndirectX = 0xC1;
const uint8_t cOpCMPIndirectY = 0xD1;

const uint8_t cOpCPXImmediate = 0xE0;
const uint8_t cOpCPXZeroPage = 0xE4;
const uint8_t cOpCPXAbsolute = 0xEC;

const uint8_t cOpCPYImmediate = 0xC0;
const uint8_t cOpCPYZeroPage = 0xC4;
const uint8_t cOpCPYAbsolute = 0xCC;

const uint8_t cOpASLAccumulator = 0x0A;
const uint8_t cOpASLZeroPage = 0x06;
const uint8_t cOpASLZeroPageX = 0x16;
const uint8_t cOpASLAbsolute = 0x0E;
const uint8_t cOpASLAbsoluteX = 0x1E;

const uint8_t cOpLSRAccumulator = 0x4A;
const uint8_t cOpLSRZeroPage = 0x46;
const uint8_t cOpLSRZeroPageX = 0x56;
const uint8_t cOpLSRAbsolute = 0x4E;
const uint8_t cOpLSRAbsoluteX = 0x5E;

const uint8_t cOpROLAccumulator = 0x2A;
const uint8_t cOpROLZeroPage = 0x26;
const uint8_t cOpROLZeroPageX = 0x36;
const uint8_t cOpROLAbsolute = 0x2E;
const uint8_t cOpROLAbsoluteX = 0x3E;

const uint8_t cOpRORAccumulator = 0x6A;
const uint8_t cOpRORZeroPage = 0x66;
const uint8_t cOpRORZeroPageX = 0x76;
const uint8_t cOpRORAbsolute = 0x6E;
const uint8_t cOpRORAbsoluteX = 0x7E;

const uint8_t cOpINCZeroPage = 0xE6;
const uint8_t cOpINCZeroPageX = 0xF6;
const uint8_t cOpINCAbsolute = 0xEE;
const uint8_t cOpINCAbsoluteX = 0xFE;

const uint8_t cOpDECZeroPage = 0xC6;
const uint8_t cOpDECZeroPageX = 0xD6;
const uint8_t cOpDECAbsolute = 0xCE;
const uint8_t cOpDECAbsoluteX = 0xDE;

const uint8_t cOpINX = 0xE8;
const uint8_t cOpDEX = 0xCA;
const uint8_t cOpINY = 0xC8;
const uint8_t cOpDEY = 0x88;

const uint8_t cOpTAX = 0xAA;
const uint8_t cOpTXA = 0x8A;
const uint8_t cOpTAY = 0xA8;
const uint8_t cOpTYA = 0x98;
const uint8_t cOpTSX = 0xBA;
const uint8_t cOpTXS = 0x9A;

const uint8_t cOpLDAImmediate = 0xA9;
const uint8_t cOpLDAZeroPage = 0xA5;
const uint8_t cOpSTAZeroPage = 0x85;
const uint8_t cOpSTAAbsoluteY = 0x99;

const uint8_t cOpBPL = 0x10;
const uint8_t cOpBMI = 0x30;
const uint8_t cOpBVC = 0x50;
const uint8_t cOpBVS = 0x70;
const uint8_t cOpBCC = 0x90;
const uint8_t cOpBCS = 0xB0;
const uint8_t cOpBNE = 0xD0;
const uint8_t cOpBEQ = 0xF0;

const uint8_t cOpCLC = 0x18;
const uint8_t cOpSEC = 0x38;
const uint8_t cOpCLI = 0x58;
const uint8_t cOpSEI = 0x78;
const uint8_t cOpCLV = 0xB8;
const uint8_t cOpCLD = 0xD8;
const uint8_t cOpSED = 0xF8;

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

bool CPU6502::halted() const { return m_halted; }

bool CPU6502::run(size_t maxInstructions) {
    for (size_t i = 0; i < maxInstructions && !m_halted; ++i) {
        executeInstruction();
    }
    return m_halted;
}

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
    m_halted = false;
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

bool aluZero(uint8_t d) { return d == 0; }
bool aluNegative(uint8_t d) { return (d & 0x80) != 0; }
bool aluOverflow(uint8_t a, uint8_t b, uint8_t d) { return ((~(a ^ b)) & (a ^ d) & 0x80) != 0; }

bool evaluateBranchCondition(uint8_t opcode, bool cFlag, bool zFlag, bool nFlag, bool vFlag) {
    switch (opcode) {
    case cOpBPL:
        return !nFlag;
    case cOpBMI:
        return nFlag;
    case cOpBVC:
        return !vFlag;
    case cOpBVS:
        return vFlag;
    case cOpBCC:
        return !cFlag;
    case cOpBCS:
        return cFlag;
    case cOpBNE:
        return !zFlag;
    case cOpBEQ:
        return zFlag;
    default:
        return false; // unreachable: only called for the 8 branch opcodes
    }
}
} // namespace

void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle. m_aluFunction is the
    // function-select line (F) choosing which combinational path drives D/CO.
    m_aluOutput = m_alu.execute(m_aluA, m_aluB, m_aluFunction, m_aluCarryIn);

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
    case cOpSBCImmediate:
    case cOpANDImmediate:
    case cOpORAImmediate:
    case cOpEORImmediate:
    case cOpCMPImmediate:
    case cOpCPXImmediate:
    case cOpCPYImmediate:
        captureReadImmediate();
        break;
    case cOpADCAbsolute:
    case cOpSBCAbsolute:
    case cOpANDAbsolute:
    case cOpORAAbsolute:
    case cOpEORAbsolute:
    case cOpCMPAbsolute:
    case cOpCPXAbsolute:
    case cOpCPYAbsolute:
        captureReadAbsolute();
        break;
    case cOpADCZeroPage:
    case cOpSBCZeroPage:
    case cOpANDZeroPage:
    case cOpORAZeroPage:
    case cOpEORZeroPage:
    case cOpCMPZeroPage:
    case cOpCPXZeroPage:
    case cOpCPYZeroPage:
        captureReadZeroPage();
        break;
    case cOpADCZeroPageX:
    case cOpSBCZeroPageX:
    case cOpANDZeroPageX:
    case cOpORAZeroPageX:
    case cOpEORZeroPageX:
    case cOpCMPZeroPageX:
        captureReadZeroPageX();
        break;
    case cOpADCAbsoluteX:
    case cOpSBCAbsoluteX:
    case cOpANDAbsoluteX:
    case cOpORAAbsoluteX:
    case cOpEORAbsoluteX:
    case cOpCMPAbsoluteX:
        captureReadAbsoluteX();
        break;
    case cOpADCAbsoluteY:
    case cOpSBCAbsoluteY:
    case cOpANDAbsoluteY:
    case cOpORAAbsoluteY:
    case cOpEORAbsoluteY:
    case cOpCMPAbsoluteY:
        captureReadAbsoluteY();
        break;
    case cOpADCIndirectX:
    case cOpSBCIndirectX:
    case cOpANDIndirectX:
    case cOpORAIndirectX:
    case cOpEORIndirectX:
    case cOpCMPIndirectX:
        captureReadIndirectX();
        break;
    case cOpADCIndirectY:
    case cOpSBCIndirectY:
    case cOpANDIndirectY:
    case cOpORAIndirectY:
    case cOpEORIndirectY:
    case cOpCMPIndirectY:
        captureReadIndirectY();
        break;
    case cOpASLAccumulator:
    case cOpLSRAccumulator:
    case cOpROLAccumulator:
    case cOpRORAccumulator:
        captureShiftAccumulator();
        break;
    case cOpASLZeroPage:
    case cOpLSRZeroPage:
    case cOpROLZeroPage:
    case cOpRORZeroPage:
    case cOpINCZeroPage:
    case cOpDECZeroPage:
        captureRmwZeroPage();
        break;
    case cOpASLZeroPageX:
    case cOpLSRZeroPageX:
    case cOpROLZeroPageX:
    case cOpRORZeroPageX:
    case cOpINCZeroPageX:
    case cOpDECZeroPageX:
        captureRmwZeroPageX();
        break;
    case cOpASLAbsolute:
    case cOpLSRAbsolute:
    case cOpROLAbsolute:
    case cOpRORAbsolute:
    case cOpINCAbsolute:
    case cOpDECAbsolute:
        captureRmwAbsolute();
        break;
    case cOpASLAbsoluteX:
    case cOpLSRAbsoluteX:
    case cOpROLAbsoluteX:
    case cOpRORAbsoluteX:
    case cOpINCAbsoluteX:
    case cOpDECAbsoluteX:
        captureRmwAbsoluteX();
        break;
    case cOpINX:
    case cOpDEX:
    case cOpINY:
    case cOpDEY:
        captureImpliedIncDec();
        break;
    case cOpTAX:
    case cOpTXA:
    case cOpTAY:
    case cOpTYA:
    case cOpTSX:
    case cOpTXS:
        captureImpliedTransfer();
        break;
    case cOpCLC:
    case cOpSEC:
    case cOpCLI:
    case cOpSEI:
    case cOpCLV:
    case cOpCLD:
    case cOpSED:
        captureImpliedFlagOp();
        break;
    case cOpLDAImmediate:
        captureLoadImmediate();
        break;
    case cOpLDAZeroPage:
        captureLoadZeroPage();
        break;
    case cOpSTAZeroPage:
        captureStoreZeroPage();
        break;
    case cOpSTAAbsoluteY:
        captureStoreAbsoluteY();
        break;
    case cOpBPL:
    case cOpBMI:
    case cOpBVC:
    case cOpBVS:
    case cOpBCC:
    case cOpBCS:
    case cOpBNE:
    case cOpBEQ:
        captureBranch();
        break;
    case cOpBRK:
        captureBRK();
        break;
    case cOpJSR:
        captureJSR();
        break;
    case cOpRTS:
        captureRTS();
        break;
    case cOpPHA:
    case cOpPHP:
        captureImpliedPush();
        break;
    case cOpPLA:
    case cOpPLP:
        captureImpliedPull();
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
    case cOpSBCImmediate:
    case cOpANDImmediate:
    case cOpORAImmediate:
    case cOpEORImmediate:
    case cOpCMPImmediate:
    case cOpCPXImmediate:
    case cOpCPYImmediate:
        commitReadImmediate();
        break;
    case cOpADCAbsolute:
    case cOpSBCAbsolute:
    case cOpANDAbsolute:
    case cOpORAAbsolute:
    case cOpEORAbsolute:
    case cOpCMPAbsolute:
    case cOpCPXAbsolute:
    case cOpCPYAbsolute:
        commitReadAbsolute();
        break;
    case cOpADCZeroPage:
    case cOpSBCZeroPage:
    case cOpANDZeroPage:
    case cOpORAZeroPage:
    case cOpEORZeroPage:
    case cOpCMPZeroPage:
    case cOpCPXZeroPage:
    case cOpCPYZeroPage:
        commitReadZeroPage();
        break;
    case cOpADCZeroPageX:
    case cOpSBCZeroPageX:
    case cOpANDZeroPageX:
    case cOpORAZeroPageX:
    case cOpEORZeroPageX:
    case cOpCMPZeroPageX:
        commitReadZeroPageX();
        break;
    case cOpADCAbsoluteX:
    case cOpSBCAbsoluteX:
    case cOpANDAbsoluteX:
    case cOpORAAbsoluteX:
    case cOpEORAbsoluteX:
    case cOpCMPAbsoluteX:
        commitReadAbsoluteX();
        break;
    case cOpADCAbsoluteY:
    case cOpSBCAbsoluteY:
    case cOpANDAbsoluteY:
    case cOpORAAbsoluteY:
    case cOpEORAbsoluteY:
    case cOpCMPAbsoluteY:
        commitReadAbsoluteY();
        break;
    case cOpADCIndirectX:
    case cOpSBCIndirectX:
    case cOpANDIndirectX:
    case cOpORAIndirectX:
    case cOpEORIndirectX:
    case cOpCMPIndirectX:
        commitReadIndirectX();
        break;
    case cOpADCIndirectY:
    case cOpSBCIndirectY:
    case cOpANDIndirectY:
    case cOpORAIndirectY:
    case cOpEORIndirectY:
    case cOpCMPIndirectY:
        commitReadIndirectY();
        break;
    case cOpASLAccumulator:
    case cOpLSRAccumulator:
    case cOpROLAccumulator:
    case cOpRORAccumulator:
        commitShiftAccumulator();
        break;
    case cOpASLZeroPage:
    case cOpLSRZeroPage:
    case cOpROLZeroPage:
    case cOpRORZeroPage:
    case cOpINCZeroPage:
    case cOpDECZeroPage:
        commitRmwZeroPage();
        break;
    case cOpASLZeroPageX:
    case cOpLSRZeroPageX:
    case cOpROLZeroPageX:
    case cOpRORZeroPageX:
    case cOpINCZeroPageX:
    case cOpDECZeroPageX:
        commitRmwZeroPageX();
        break;
    case cOpASLAbsolute:
    case cOpLSRAbsolute:
    case cOpROLAbsolute:
    case cOpRORAbsolute:
    case cOpINCAbsolute:
    case cOpDECAbsolute:
        commitRmwAbsolute();
        break;
    case cOpASLAbsoluteX:
    case cOpLSRAbsoluteX:
    case cOpROLAbsoluteX:
    case cOpRORAbsoluteX:
    case cOpINCAbsoluteX:
    case cOpDECAbsoluteX:
        commitRmwAbsoluteX();
        break;
    case cOpINX:
    case cOpDEX:
    case cOpINY:
    case cOpDEY:
        commitImpliedIncDec();
        break;
    case cOpTAX:
    case cOpTXA:
    case cOpTAY:
    case cOpTYA:
    case cOpTSX:
    case cOpTXS:
        commitImpliedTransfer();
        break;
    case cOpCLC:
    case cOpSEC:
    case cOpCLI:
    case cOpSEI:
    case cOpCLV:
    case cOpCLD:
    case cOpSED:
        commitImpliedFlagOp();
        break;
    case cOpLDAImmediate:
        commitLoadImmediate();
        break;
    case cOpLDAZeroPage:
        commitLoadZeroPage();
        break;
    case cOpSTAZeroPage:
        commitStoreZeroPage();
        break;
    case cOpSTAAbsoluteY:
        commitStoreAbsoluteY();
        break;
    case cOpBPL:
    case cOpBMI:
    case cOpBVC:
    case cOpBVS:
    case cOpBCC:
    case cOpBCS:
    case cOpBNE:
    case cOpBEQ:
        commitBranch();
        break;
    case cOpBRK:
        commitBRK();
        break;
    case cOpJSR:
        commitJSR();
        break;
    case cOpRTS:
        commitRTS();
        break;
    case cOpPHA:
    case cOpPHP:
        commitImpliedPush();
        break;
    case cOpPLA:
    case cOpPLP:
        commitImpliedPull();
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

EffectiveAddress CPU6502::relativeAddress(uint16_t base, int8_t offset) {
    auto address = static_cast<uint16_t>(base + offset);
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

void CPU6502::beginBinaryAluOp(AluOp aluOp, uint8_t regValue, uint8_t operand) {
    m_aluA = regValue;
    m_aluB = operand;
    m_aluCarryIn = CFlag();
    switch (aluOp) {
    case AluOp::ADC:
        m_aluFunction = AluFunction::ADD;
        break;
    case AluOp::SBC:
        m_aluFunction = AluFunction::ADD;
        m_aluB = static_cast<uint8_t>(~operand);
        break;
    case AluOp::AND:
        m_aluFunction = AluFunction::AND;
        break;
    case AluOp::ORA:
        m_aluFunction = AluFunction::OR;
        break;
    case AluOp::EOR:
        m_aluFunction = AluFunction::XOR;
        break;
    case AluOp::CMP:
        m_aluFunction = AluFunction::ADD;
        m_aluB = static_cast<uint8_t>(~operand);
        m_aluCarryIn = true; // compare never borrows in
        break;
    default:
        break; // unreachable: only binary ops routed here
    }
}

void CPU6502::applyBinaryAluOp(uint8_t operand) {
    switch (m_IR) {
    case cOpADCImmediate:
    case cOpADCZeroPage:
    case cOpADCZeroPageX:
    case cOpADCAbsolute:
    case cOpADCAbsoluteX:
    case cOpADCAbsoluteY:
    case cOpADCIndirectX:
    case cOpADCIndirectY:
        beginBinaryAluOp(AluOp::ADC, m_A, operand);
        break;
    case cOpSBCImmediate:
    case cOpSBCZeroPage:
    case cOpSBCZeroPageX:
    case cOpSBCAbsolute:
    case cOpSBCAbsoluteX:
    case cOpSBCAbsoluteY:
    case cOpSBCIndirectX:
    case cOpSBCIndirectY:
        beginBinaryAluOp(AluOp::SBC, m_A, operand);
        break;
    case cOpANDImmediate:
    case cOpANDZeroPage:
    case cOpANDZeroPageX:
    case cOpANDAbsolute:
    case cOpANDAbsoluteX:
    case cOpANDAbsoluteY:
    case cOpANDIndirectX:
    case cOpANDIndirectY:
        beginBinaryAluOp(AluOp::AND, m_A, operand);
        break;
    case cOpORAImmediate:
    case cOpORAZeroPage:
    case cOpORAZeroPageX:
    case cOpORAAbsolute:
    case cOpORAAbsoluteX:
    case cOpORAAbsoluteY:
    case cOpORAIndirectX:
    case cOpORAIndirectY:
        beginBinaryAluOp(AluOp::ORA, m_A, operand);
        break;
    case cOpEORImmediate:
    case cOpEORZeroPage:
    case cOpEORZeroPageX:
    case cOpEORAbsolute:
    case cOpEORAbsoluteX:
    case cOpEORAbsoluteY:
    case cOpEORIndirectX:
    case cOpEORIndirectY:
        beginBinaryAluOp(AluOp::EOR, m_A, operand);
        break;
    case cOpCMPImmediate:
    case cOpCMPZeroPage:
    case cOpCMPZeroPageX:
    case cOpCMPAbsolute:
    case cOpCMPAbsoluteX:
    case cOpCMPAbsoluteY:
    case cOpCMPIndirectX:
    case cOpCMPIndirectY:
        beginBinaryAluOp(AluOp::CMP, m_A, operand);
        break;
    case cOpCPXImmediate:
    case cOpCPXZeroPage:
    case cOpCPXAbsolute:
        beginBinaryAluOp(AluOp::CMP, m_X, operand);
        break;
    case cOpCPYImmediate:
    case cOpCPYZeroPage:
    case cOpCPYAbsolute:
        beginBinaryAluOp(AluOp::CMP, m_Y, operand);
        break;
    default:
        break; // Unreachable: only called for opcodes routed through the binary-ALU family.
    }
}

void CPU6502::commitBinaryAluResult() {
    switch (m_IR) {
    case cOpADCImmediate:
    case cOpADCZeroPage:
    case cOpADCZeroPageX:
    case cOpADCAbsolute:
    case cOpADCAbsoluteX:
    case cOpADCAbsoluteY:
    case cOpADCIndirectX:
    case cOpADCIndirectY:
    case cOpSBCImmediate:
    case cOpSBCZeroPage:
    case cOpSBCZeroPageX:
    case cOpSBCAbsolute:
    case cOpSBCAbsoluteX:
    case cOpSBCAbsoluteY:
    case cOpSBCIndirectX:
    case cOpSBCIndirectY:
        // ADC/SBC: write A; C, Z, V, N all follow the arithmetic result.
        A(m_aluOutput.value);
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        VFlag(aluOverflow(m_aluA, m_aluB, m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpANDImmediate:
    case cOpANDZeroPage:
    case cOpANDZeroPageX:
    case cOpANDAbsolute:
    case cOpANDAbsoluteX:
    case cOpANDAbsoluteY:
    case cOpANDIndirectX:
    case cOpANDIndirectY:
    case cOpORAImmediate:
    case cOpORAZeroPage:
    case cOpORAZeroPageX:
    case cOpORAAbsolute:
    case cOpORAAbsoluteX:
    case cOpORAAbsoluteY:
    case cOpORAIndirectX:
    case cOpORAIndirectY:
    case cOpEORImmediate:
    case cOpEORZeroPage:
    case cOpEORZeroPageX:
    case cOpEORAbsolute:
    case cOpEORAbsoluteX:
    case cOpEORAbsoluteY:
    case cOpEORIndirectX:
    case cOpEORIndirectY:
        // AND/ORA/EOR: write A; only Z, N follow. C and V are real 6502
        // behavior left untouched by logical ops, not an oversight.
        A(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpCMPImmediate:
    case cOpCMPZeroPage:
    case cOpCMPZeroPageX:
    case cOpCMPAbsolute:
    case cOpCMPAbsoluteX:
    case cOpCMPAbsoluteY:
    case cOpCMPIndirectX:
    case cOpCMPIndirectY:
    case cOpCPXImmediate:
    case cOpCPXZeroPage:
    case cOpCPXAbsolute:
    case cOpCPYImmediate:
    case cOpCPYZeroPage:
    case cOpCPYAbsolute:
        // CMP/CPX/CPY: write nothing back; only C, Z, N follow.
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    default:
        break; // Unreachable: only called for opcodes routed through the binary-ALU family.
    }
}

void CPU6502::captureReadImmediate() { applyBinaryAluOp(m_bus.read(m_PC)); }

void CPU6502::commitReadImmediate() {
    m_PC++;
    commitBinaryAluResult();
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureReadAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(m_PC)) << 8;
        break;
    case CpuStep::T3:
        applyBinaryAluOp(m_bus.read(m_addrLatch));
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}

void CPU6502::commitReadAbsolute() {
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
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}

void CPU6502::captureReadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        applyBinaryAluOp(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitReadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureReadZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        applyBinaryAluOp(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}

void CPU6502::commitReadZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}

void CPU6502::captureReadAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base =
            static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_X);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            applyBinaryAluOp(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        applyBinaryAluOp(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitReadAbsoluteX() {
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
            commitBinaryAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::captureReadAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base =
            static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            applyBinaryAluOp(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        applyBinaryAluOp(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitReadAbsoluteY() {
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
            commitBinaryAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::captureReadIndirectX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        m_effAddr = m_bus.read(m_addrLatch);
        break;
    case CpuStep::T4:
        m_effAddr |= static_cast<uint16_t>(m_bus.read((m_addrLatch + 1) & 0xFF)) << 8;
        break;
    case CpuStep::T5:
        applyBinaryAluOp(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitReadIndirectX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::captureReadIndirectY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_effAddr = m_bus.read(m_addrLatch);
        break;
    case CpuStep::T3: {
        auto pointerHigh = static_cast<uint16_t>(m_bus.read((m_addrLatch + 1) & 0xFF));
        auto base = static_cast<uint16_t>(m_effAddr | (pointerHigh << 8));
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T4:
        if (!m_pageCrossed) {
            applyBinaryAluOp(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T5:
        applyBinaryAluOp(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitReadIndirectY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T5;
        } else {
            commitBinaryAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T5:
        commitBinaryAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::beginUnaryAluOp(AluOp aluOp, uint8_t value) {
    m_aluB = value;
    switch (aluOp) {
    case AluOp::ASL:
        m_aluFunction = AluFunction::SHL;
        m_aluCarryIn = false; // unused by SHL
        break;
    case AluOp::LSR:
        m_aluFunction = AluFunction::SHR;
        m_aluCarryIn = false; // unused by SHR
        break;
    case AluOp::ROL:
        m_aluFunction = AluFunction::ROL;
        m_aluCarryIn = CFlag();
        break;
    case AluOp::ROR:
        m_aluFunction = AluFunction::ROR;
        m_aluCarryIn = CFlag();
        break;
    case AluOp::INC:
        m_aluFunction = AluFunction::ADD;
        m_aluA = 0x01;
        m_aluCarryIn = false;
        break;
    case AluOp::DEC:
        m_aluFunction = AluFunction::ADD;
        m_aluA = 0xFF;
        m_aluCarryIn = false;
        break;
    default:
        break; // unreachable: only unary ops routed here
    }
}

void CPU6502::applyUnaryAluOp(uint8_t value) {
    switch (m_IR) {
    case cOpASLAccumulator:
    case cOpASLZeroPage:
    case cOpASLZeroPageX:
    case cOpASLAbsolute:
    case cOpASLAbsoluteX:
        beginUnaryAluOp(AluOp::ASL, value);
        break;
    case cOpLSRAccumulator:
    case cOpLSRZeroPage:
    case cOpLSRZeroPageX:
    case cOpLSRAbsolute:
    case cOpLSRAbsoluteX:
        beginUnaryAluOp(AluOp::LSR, value);
        break;
    case cOpROLAccumulator:
    case cOpROLZeroPage:
    case cOpROLZeroPageX:
    case cOpROLAbsolute:
    case cOpROLAbsoluteX:
        beginUnaryAluOp(AluOp::ROL, value);
        break;
    case cOpRORAccumulator:
    case cOpRORZeroPage:
    case cOpRORZeroPageX:
    case cOpRORAbsolute:
    case cOpRORAbsoluteX:
        beginUnaryAluOp(AluOp::ROR, value);
        break;
    case cOpINCZeroPage:
    case cOpINCZeroPageX:
    case cOpINCAbsolute:
    case cOpINCAbsoluteX:
        beginUnaryAluOp(AluOp::INC, value);
        break;
    case cOpDECZeroPage:
    case cOpDECZeroPageX:
    case cOpDECAbsolute:
    case cOpDECAbsoluteX:
        beginUnaryAluOp(AluOp::DEC, value);
        break;
    default:
        break; // Unreachable: only called for opcodes routed through the unary-ALU family.
    }
}

void CPU6502::commitUnaryAluFlags() {
    switch (m_IR) {
    case cOpASLAccumulator:
    case cOpASLZeroPage:
    case cOpASLZeroPageX:
    case cOpASLAbsolute:
    case cOpASLAbsoluteX:
    case cOpLSRAccumulator:
    case cOpLSRZeroPage:
    case cOpLSRZeroPageX:
    case cOpLSRAbsolute:
    case cOpLSRAbsoluteX:
    case cOpROLAccumulator:
    case cOpROLZeroPage:
    case cOpROLZeroPageX:
    case cOpROLAbsolute:
    case cOpROLAbsoluteX:
    case cOpRORAccumulator:
    case cOpRORZeroPage:
    case cOpRORZeroPageX:
    case cOpRORAbsolute:
    case cOpRORAbsoluteX:
        // ASL/LSR/ROL/ROR: C, Z, N follow the shift result. V untouched.
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpINCZeroPage:
    case cOpINCZeroPageX:
    case cOpINCAbsolute:
    case cOpINCAbsoluteX:
    case cOpDECZeroPage:
    case cOpDECZeroPageX:
    case cOpDECAbsolute:
    case cOpDECAbsoluteX:
        // INC/DEC: only Z, N follow. C and V are real 6502 behavior left
        // untouched, not an oversight.
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    default:
        break; // Unreachable: only called for opcodes routed through the unary-ALU family.
    }
}

void CPU6502::captureShiftAccumulator() { applyUnaryAluOp(m_A); }

void CPU6502::commitShiftAccumulator() {
    A(m_aluOutput.value);
    commitUnaryAluFlags();
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureRmwZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        applyUnaryAluOp(m_bus.read(m_addrLatch));
        break;
    case CpuStep::T3:
    case CpuStep::T4:
        break; // idle: stand-in for the dummy write-back; the ALU result is already available
               // combinationally
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitRmwZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_bus.write(m_addrLatch, m_aluOutput.value);
        commitUnaryAluFlags();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::captureRmwZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        applyUnaryAluOp(m_bus.read(m_addrLatch));
        break;
    case CpuStep::T4:
    case CpuStep::T5:
        break; // idle: stand-in for the dummy write-back; the ALU result is already available
               // combinationally
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitRmwZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_bus.write(m_addrLatch, m_aluOutput.value);
        commitUnaryAluFlags();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::captureRmwAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(m_PC)) << 8;
        break;
    case CpuStep::T3:
        applyUnaryAluOp(m_bus.read(m_addrLatch));
        break;
    case CpuStep::T4:
    case CpuStep::T5:
        break; // idle: stand-in for the dummy write-back; the ALU result is already available
               // combinationally
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitRmwAbsolute() {
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
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_bus.write(m_addrLatch, m_aluOutput.value);
        commitUnaryAluFlags();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::captureRmwAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base =
            static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        // RMW absolute,X timing is fixed at 7 cycles on real hardware, so
        // .pageCrossed is deliberately not consulted here (unlike the
        // read-only family) -- see captureReadAbsoluteX for the contrast.
        EffectiveAddress resolved = indexedAddress(base, m_X);
        m_effAddr = resolved.address;
        break;
    }
    case CpuStep::T3:
        break; // idle: stand-in for the real hardware's wrong-address read before the index fixup
    case CpuStep::T4:
        applyUnaryAluOp(m_bus.read(m_effAddr));
        break;
    case CpuStep::T5:
    case CpuStep::T6:
        break; // idle: stand-in for the dummy write-back; the ALU result is already available
               // combinationally
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}

void CPU6502::commitRmwAbsoluteX() {
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
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_cpuStep = CpuStep::T6;
        break;
    case CpuStep::T6:
        m_bus.write(m_effAddr, m_aluOutput.value);
        commitUnaryAluFlags();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}

void CPU6502::captureImpliedIncDec() {
    switch (m_IR) {
    case cOpINX:
        beginUnaryAluOp(AluOp::INC, m_X);
        break;
    case cOpDEX:
        beginUnaryAluOp(AluOp::DEC, m_X);
        break;
    case cOpINY:
        beginUnaryAluOp(AluOp::INC, m_Y);
        break;
    case cOpDEY:
        beginUnaryAluOp(AluOp::DEC, m_Y);
        break;
    default:
        break; // Unreachable: this handler is only invoked for INX/DEX/INY/DEY.
    }
}

void CPU6502::commitImpliedIncDec() {
    switch (m_IR) {
    case cOpINX:
    case cOpDEX:
        X(m_aluOutput.value);
        break;
    case cOpINY:
    case cOpDEY:
        Y(m_aluOutput.value);
        break;
    default:
        break; // Unreachable: this handler is only invoked for INX/DEX/INY/DEY.
    }
    ZFlag(aluZero(m_aluOutput.value));
    NFlag(aluNegative(m_aluOutput.value));
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureImpliedTransfer() {
    uint8_t source = 0;
    switch (m_IR) {
    case cOpTAX:
    case cOpTAY:
        source = m_A;
        break;
    case cOpTXA:
    case cOpTXS:
        source = m_X;
        break;
    case cOpTYA:
        source = m_Y;
        break;
    case cOpTSX:
        source = m_SP;
        break;
    default:
        break; // Unreachable: this handler is only invoked for TAX/TXA/TAY/TYA/TSX/TXS.
    }
    m_aluA = 0;
    m_aluB = source;
    m_aluFunction = AluFunction::OR;
}

void CPU6502::commitImpliedTransfer() {
    switch (m_IR) {
    case cOpTAX:
    case cOpTSX:
        X(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpTAY:
        Y(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpTXA:
    case cOpTYA:
        A(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
    case cOpTXS:
        // TXS: SP loaded from X, but real 6502 hardware leaves every flag
        // untouched -- unlike every other transfer, which sets Z/N.
        SP(m_aluOutput.value);
        break;
    default:
        break; // Unreachable: this handler is only invoked for TAX/TXA/TAY/TYA/TSX/TXS.
    }
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureImpliedFlagOp() {
    // No ALU involvement, no bus access: nothing to capture.
}

void CPU6502::commitImpliedFlagOp() {
    switch (m_IR) {
    case cOpCLC:
        CFlag(false);
        break;
    case cOpSEC:
        CFlag(true);
        break;
    case cOpCLI:
        IFlag(false);
        break;
    case cOpSEI:
        IFlag(true);
        break;
    case cOpCLV:
        VFlag(false);
        break;
    case cOpCLD:
        DFlag(false);
        break;
    case cOpSED:
        DFlag(true);
        break;
    default:
        break; // Unreachable: this handler is only invoked for CLC/SEC/CLI/SEI/CLV/CLD/SED.
    }
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureLoadImmediate() {
    m_aluA = 0;
    m_aluB = m_bus.read(m_PC);
    m_aluFunction = AluFunction::OR;
}

void CPU6502::commitLoadImmediate() {
    m_PC++;
    A(m_aluOutput.value);
    ZFlag(aluZero(m_aluOutput.value));
    NFlag(aluNegative(m_aluOutput.value));
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureLoadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_aluA = 0;
        m_aluB = m_bus.read(m_addrLatch);
        m_aluFunction = AluFunction::OR;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitLoadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        A(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureStoreZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        break; // idle: the store itself happens in commit, where m_A is authoritative
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitStoreZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_bus.write(m_addrLatch, m_A);
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureStoreAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base =
            static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        // STA absolute,Y timing is fixed at 5 cycles on real hardware (a
        // store can't shortcut the extra cycle the way a read can), so
        // .pageCrossed is deliberately not consulted -- see
        // captureRmwAbsoluteX for the same reasoning applied to RMW.
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        break;
    }
    case CpuStep::T3:
        break; // idle: stand-in for the real hardware's wrong-address read before the index fixup
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::commitStoreAbsoluteY() {
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
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_bus.write(m_effAddr, m_A);
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::captureBranch() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_branchOffset = static_cast<int8_t>(m_bus.read(m_PC));
        m_branchTaken = evaluateBranchCondition(m_IR, CFlag(), ZFlag(), NFlag(), VFlag());
        break;
    case CpuStep::T2: {
        EffectiveAddress resolved = relativeAddress(m_PC, m_branchOffset);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        break; // idle: stand-in for the dummy read while the high byte is fixed up
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::commitBranch() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = m_branchTaken ? CpuStep::T2 : CpuStep::T0;
        break;
    case CpuStep::T2:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T3;
        } else {
            m_PC = m_effAddr;
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T3:
        m_PC = m_effAddr;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::captureBRK() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read: padding byte after BRK, discarded
        break;
    case CpuStep::T2:
    case CpuStep::T3:
    case CpuStep::T4:
        break; // idle: the pushes happen in commit, where m_SP is authoritative
    case CpuStep::T5:
        m_addrLatch = m_bus.read(cBRKVector);
        break;
    case CpuStep::T6:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(static_cast<uint16_t>(cBRKVector + 1)))
                       << 8;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}

void CPU6502::commitBRK() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC >> 8));
        m_SP--;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC & 0xFF));
        m_SP--;
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        BFlag(true);
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), P());
        m_SP--;
        IFlag(true);
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_cpuStep = CpuStep::T6;
        break;
    case CpuStep::T6:
        m_PC = m_addrLatch;
        m_halted = true;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}

void CPU6502::captureJSR() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
    case CpuStep::T3:
    case CpuStep::T4:
        break; // idle: internal S predecrement (T2) and the two pushes (T3/T4) are driven from
               // commit, where m_PC/m_SP are authoritative
    case CpuStep::T5:
        m_addrLatch = static_cast<uint16_t>((m_addrLatch & 0x00FF) |
                                            (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitJSR() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        // PC now points at the operand's high byte -- that address (not the
        // next instruction's) is what JSR pushes below.
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC >> 8));
        m_SP--;
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC & 0xFF));
        m_SP--;
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_PC = m_addrLatch;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::captureRTS() {
    switch (m_cpuStep) {
    case CpuStep::T1:
    case CpuStep::T2:
        break; // idle: dummy read of the padding byte (T1) and the S increment before the first
               // pull (T2)
    case CpuStep::T3:
        m_addrLatch = m_bus.read(static_cast<uint16_t>(0x0100 + m_SP));
        break;
    case CpuStep::T4:
        m_addrLatch = static_cast<uint16_t>(
            (m_addrLatch & 0x00FF) |
            (static_cast<uint16_t>(m_bus.read(static_cast<uint16_t>(0x0100 + m_SP))) << 8));
        break;
    case CpuStep::T5:
        break; // idle: PC is loaded from m_addrLatch (+1) in commit
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitRTS() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        // Real hardware's dummy read of the byte after the opcode does not
        // advance PC -- PC is about to be overwritten from the stack anyway.
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_SP++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_SP++;
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        // RTS pulls the address of JSR's own last byte, so add one to land
        // on the instruction that actually follows the call.
        m_PC = static_cast<uint16_t>(m_addrLatch + 1);
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::captureImpliedPush() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read: internal-operation cycle, discarded
        break;
    case CpuStep::T2:
        break; // idle: the push happens in commit, where m_SP is authoritative
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T2.
    }
}

void CPU6502::commitImpliedPush() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2: {
        uint8_t value = A();
        if (m_IR == cOpPHP) {
            BFlag(true); // no physical B flip-flop -- synthesized on every push, same as BRK
            value = P();
        }
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), value);
        m_SP--;
        m_cpuStep = CpuStep::T0;
        break;
    }
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T2.
    }
}

void CPU6502::captureImpliedPull() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read: internal-operation cycle, discarded
        break;
    case CpuStep::T2:
        break; // idle: S increment happens in commit, where m_SP is authoritative
    case CpuStep::T3:
        m_addrLatch = m_bus.read(static_cast<uint16_t>(0x0100 + m_SP));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::commitImpliedPull() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_SP++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3: {
        auto pulled = static_cast<uint8_t>(m_addrLatch);
        if (m_IR == cOpPLA) {
            m_A = pulled;
            ZFlag(aluZero(pulled));
            NFlag(aluNegative(pulled));
        } else {
            P(pulled);
        }
        m_cpuStep = CpuStep::T0;
        break;
    }
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}
