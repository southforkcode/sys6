#pragma once

#include "alu.h"
#include "cpu.h"

#include <bitset>
#include <cstddef>
#include <cstdint>

class Bus;

enum class CpuStep : uint8_t { T0, T1, T2, T3, T4, T5, T6 };
enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };
enum class AluOp : uint8_t { ADC, SBC, AND, ORA, EOR, CMP, ASL, LSR, ROL, ROR, INC, DEC, BIT };

struct EffectiveAddress {
    uint16_t address;
    bool pageCrossed;
};

class CPU6502 : public CPU {
public:
    explicit CPU6502(Bus &bus);

    void reset() override;

    // Arms the reset sequence (m_resetActive = true, m_cpuStep = T0)
    // without pumping it to completion -- for a driver that wants to
    // observe/trace the 7 T-state reset sequence one tick() at a time.
    // reset() is the convenience wrapper most callers should use instead:
    // it calls this and then pumps via tick() until the sequence
    // completes, so the CPU is fully settled by the time reset() returns.
    void beginReset();

    void executeInstruction() override;
    void tick();

    // Calls tick() repeatedly until the clock reaches the next High
    // (stable) phase. Convenience for a driver that wants to observe
    // capture-time state without manually stepping through the settling
    // phases in between -- it can still fall back to tick() for
    // phase-by-phase control when it does care about every intermediate
    // step. Always advances at least one tick, even if already at High.
    void runToClockHigh();

    CpuId id() const override;

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
    uint8_t m_A = 0;   // accumulator register
    uint8_t m_X = 0;   // index register X
    uint8_t m_Y = 0;   // index register Y
    uint16_t m_PC = 0; // program counter
    uint8_t m_SP = 0;  // stack pointer
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
    bool m_resetActive = false;

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

    // Reset sequence: models real 6502 RESET as 7 T-states (T0-T6) -- 2
    // dummy reads, 3 phantom stack decrements (no writes; real hardware
    // suppresses them), then a 2-cycle read of cResetVector. m_resetActive
    // routes onClockHigh()/onClockLow() here ahead of the normal
    // T0-opcode-fetch/m_IR dispatch.
    void captureReset();
    void commitReset();

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

    // Transfer family (TAX/TXA/TAY/TYA/TSX/TXS): 2 cycles, no bus access
    // beyond the opcode fetch, copies one register into another through the
    // ALU's OR-with-zero pass-through -- the same trick captureLoadImmediate
    // uses -- so Z/N come from the shared aluZero()/aluNegative() helpers.
    // TXS is the one exception: real 6502 hardware leaves every flag
    // untouched when loading SP, so its commit skips the flag writes.
    void captureImpliedTransfer();
    void commitImpliedTransfer();

    // Flag family (CLC/SEC/CLI/SEI/CLV/CLD/SED): 2 cycles, no bus access
    // beyond the opcode fetch, sets or clears exactly one status flag and
    // leaves every other flag and register untouched. No ALU involvement --
    // the capture stage is idle (same convention as e.g. captureRmwZeroPage).
    void captureImpliedFlagOp();
    void commitImpliedFlagOp();

    // Load/store family (LDA/LDX/LDY/STA): loads reuse the ALU's
    // combinational path (OR with a=0 passes the fetched byte through
    // unchanged) so they share the same aluZero()/aluNegative() flag
    // helpers as every other opcode; C and V are real 6502 behavior left
    // untouched. STA touches no flags and needs no ALU involvement at all.
    // One capture/commit pair per addressing-mode shape, same convention as
    // the binary-ALU family: each pair hardcodes its own index register
    // (X or Y) where the addressing mode requires one, and commit picks the
    // destination register (A/X/Y) from m_IR -- mirroring how
    // commitBinaryAluResult() picks the operation from m_IR.
    void captureLoadImmediate();
    void commitLoadImmediate();
    void captureLoadZeroPage();
    void commitLoadZeroPage();
    void captureLoadZeroPageX();
    void commitLoadZeroPageX();
    void captureLoadZeroPageY();
    void commitLoadZeroPageY();
    void captureLoadAbsolute();
    void commitLoadAbsolute();
    void captureLoadAbsoluteX();
    void commitLoadAbsoluteX();
    void captureLoadAbsoluteY();
    void commitLoadAbsoluteY();
    void captureLoadIndirectX();
    void commitLoadIndirectX();
    void captureLoadIndirectY();
    void commitLoadIndirectY();
    void commitLoadResult();
    uint8_t storeSourceValue() const;
    void captureStoreZeroPage();
    void commitStoreZeroPage();
    void captureStoreZeroPageX();
    void commitStoreZeroPageX();
    void captureStoreZeroPageY();
    void commitStoreZeroPageY();
    void captureStoreAbsolute();
    void commitStoreAbsolute();
    void captureStoreAbsoluteX();
    void commitStoreAbsoluteX();
    void captureStoreAbsoluteY();
    void commitStoreAbsoluteY();
    void captureStoreIndirectX();
    void commitStoreIndirectX();
    void captureStoreIndirectY();
    void commitStoreIndirectY();

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

    // JSR/RTS: JSR pushes the address of its own last byte (not the next
    // instruction) and jumps to its absolute operand; RTS pulls that
    // address back and adds one to land on the actual next instruction.
    void captureJSR();
    void commitJSR();
    void captureRTS();
    void commitRTS();

    // JMP: Absolute loads PC directly from its two-byte operand. Indirect
    // reads a pointer from the operand address and loads PC from the
    // 16-bit value stored there -- reproducing the real 6502 hardware bug
    // where the high-byte fetch fails to cross a page boundary if the
    // pointer's low byte is 0xFF, wrapping within the same page instead.
    void captureJMPAbsolute();
    void commitJMPAbsolute();
    void captureJMPIndirect();
    void commitJMPIndirect();

    // NOP: 2 cycles, no bus access beyond the opcode fetch, no state change
    // at all -- same shape as captureImpliedFlagOp, minus the flag write.
    void captureNOP();
    void commitNOP();

    // RTI: pulls P (B included, same wholesale restore PLP already does)
    // then PCL/PCH from the stack -- the same three-pull shape BRK's push
    // mirrors in reverse. Unlike every other pull in this codebase, the
    // reads happen directly in commit rather than being staged in capture,
    // since nothing needs to observe the pulled status byte mid-instruction.
    void captureRTI();
    void commitRTI();

    // Stack family (PHA/PHP/PLA/PLP): PHA/PHP push A/P (PHP forces B to 1,
    // same real-hardware reason BRK does); PLA/PLP pull into A/P. PLA sets
    // Z/N from the pulled byte; PLP overwrites the whole status register
    // wholesale, B included -- it restores exactly what was pushed.
    void captureImpliedPush();
    void commitImpliedPush();
    void captureImpliedPull();
    void commitImpliedPull();
};
