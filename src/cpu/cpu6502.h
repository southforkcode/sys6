#pragma once

#include "alu.h"
#include "cpu.h"

#include <bitset>
#include <cstddef>
#include <cstdint>

class Bus;

enum class CpuStep : uint8_t { T0, T1, T2, T3, T4, T5, T6 };
enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };
enum class AluOp : uint8_t { ADC, SBC, AND, ORA, EOR, CMP, ASL, LSR, ROL, ROR, INC, DEC };

struct EffectiveAddress {
    uint16_t address;
    bool pageCrossed;
};

class CPU6502 : public CPU {
public:
    explicit CPU6502(Bus &bus);

    void reset() override;
    void executeInstruction() override;
    void tick();

    // Calls tick() repeatedly until the clock reaches the next High
    // (stable) phase. Convenience for a driver that wants to observe
    // capture-time state without manually stepping through the settling
    // phases in between -- it can still fall back to tick() for
    // phase-by-phase control when it does care about every intermediate
    // step. Always advances at least one tick, even if already at High.
    void runToClockHigh();

    //--------------------------------------
    // CPU processor register getters/setters

    uint8_t A() const;
    uint8_t X() const;
    uint8_t Y() const;
    uint16_t PC() const;
    uint16_t SP() const;

    void A(uint8_t val);
    void X(uint8_t val);
    void Y(uint8_t val);
    void PC(uint16_t val);
    void SP(uint8_t val);

    //--------------------------------------
    // CPU processor flags getters/setters

    uint8_t P() const;
    void P(uint8_t val);

    bool CFlag() const;
    bool ZFlag() const;
    bool IFlag() const;
    bool DFlag() const;
    bool BFlag() const;
    bool VFlag() const;
    bool NFlag() const;

    void CFlag(bool val);
    void ZFlag(bool val);
    void IFlag(bool val);
    void DFlag(bool val);
    void BFlag(bool val);
    void VFlag(bool val);
    void NFlag(bool val);

    //--------------------------------------
    // Halt / run control

    bool halted() const;
    bool run(size_t maxInstructions);

protected:
    Bus &m_bus;
    uint8_t m_A;   // accumulator register
    uint8_t m_X;   // index register X
    uint8_t m_Y;   // index register Y
    uint16_t m_PC; // program counter
    uint8_t m_SP;  // stack pointer
    std::bitset<8> m_pFlags;

    CpuStep m_cpuStep = CpuStep::T0;
    ClockPhase m_clockPhase = ClockPhase::Low;
    uint16_t m_addrLatch = 0;
    uint16_t m_effAddr = 0;
    bool m_pageCrossed = false;
    uint8_t m_IR = 0;
    int8_t m_branchOffset = 0;
    bool m_branchTaken = false;
    bool m_halted = false;

    // The ALU is a combinational unit: it has no state of its own, but real
    // hardware wires its inputs and output through latches rather than
    // passing them as ephemeral call arguments. These members are that
    // wiring, made explicit: A/B are the operand latches, m_aluFunction is
    // the function-select line (F), m_aluCarryIn is CI, and m_aluOutput
    // holds D/CO.
    ALU m_alu;
    uint8_t m_aluA = 0;
    uint8_t m_aluB = 0;
    AluFunction m_aluFunction = AluFunction::ADD;
    bool m_aluCarryIn = false;
    AluResult m_aluOutput{};

    static EffectiveAddress indexedAddress(uint16_t base, uint8_t index);
    static EffectiveAddress relativeAddress(uint16_t base, int8_t offset);

private:
    void onClockHigh();
    void onClockLow();
    void captureOpcodeFetch();
    void commitOpcodeFetch();

    // Binary-ALU family (ADC/SBC/AND/ORA/EOR/CMP/CPX/CPY): one capture/commit
    // pair per addressing-mode shape, shared by every opcode that uses it.
    // applyBinaryAluOp()/commitBinaryAluResult() switch on m_IR to decide
    // which ALU operation, source register, and flag subset apply.
    void beginBinaryAluOp(AluOp aluOp, uint8_t regValue, uint8_t operand);
    void applyBinaryAluOp(uint8_t operand);
    void commitBinaryAluResult();
    void captureReadImmediate();
    void commitReadImmediate();
    void captureReadZeroPage();
    void commitReadZeroPage();
    void captureReadZeroPageX();
    void commitReadZeroPageX();
    void captureReadAbsolute();
    void commitReadAbsolute();
    void captureReadAbsoluteX();
    void commitReadAbsoluteX();
    void captureReadAbsoluteY();
    void commitReadAbsoluteY();
    void captureReadIndirectX();
    void commitReadIndirectX();
    void captureReadIndirectY();
    void commitReadIndirectY();

    // Unary-ALU family (ASL/LSR/ROL/ROR/INC/DEC): reads a value, transforms
    // it, writes it back -- to A for shift/rotate Accumulator mode, to
    // memory for every other mode. applyUnaryAluOp()/commitUnaryAluFlags()
    // switch on m_IR to decide the operation and flag subset.
    void beginUnaryAluOp(AluOp aluOp, uint8_t value);
    void applyUnaryAluOp(uint8_t value);
    void commitUnaryAluFlags();
    void captureShiftAccumulator();
    void commitShiftAccumulator();
    void captureRmwZeroPage();
    void commitRmwZeroPage();
    void captureRmwZeroPageX();
    void commitRmwZeroPageX();
    void captureRmwAbsolute();
    void commitRmwAbsolute();
    void captureRmwAbsoluteX();
    void commitRmwAbsoluteX();

    // Implied-register family (INX/DEX/INY/DEY): 2 cycles, no bus access
    // beyond the opcode fetch, operates directly on X or Y.
    void captureImpliedIncDec();
    void commitImpliedIncDec();

    // Load/store family (LDA/STA): LDA reuses the ALU's combinational path
    // (OR with a=0 passes the fetched byte through unchanged) so it shares
    // the same aluZero()/aluNegative() flag helpers as every other opcode;
    // C and V are real 6502 behavior left untouched. STA touches no flags
    // and needs no ALU involvement at all.
    void captureLoadImmediate();
    void commitLoadImmediate();
    void captureLoadZeroPage();
    void commitLoadZeroPage();
    void captureStoreZeroPage();
    void commitStoreZeroPage();
    void captureStoreAbsoluteY();
    void commitStoreAbsoluteY();

    // Relative branches (BEQ/BNE/BCS/BCC/BPL/BMI/BVS/BVC): one shared pair
    // for all 8, deciding the condition and target address from m_IR, the
    // same way applyBinaryAluOp() decides its operation from m_IR.
    void captureBranch();
    void commitBranch();

    // BRK (full interrupt semantics): pushes PC+2 and P (with B forced to
    // 1) onto the stack, sets the I flag, and loads PC from cBRKVector.
    // m_halted is the actual "catch a BRK" mechanism a driver uses to stop
    // issuing further instructions -- see run().
    void captureBRK();
    void commitBRK();
};
