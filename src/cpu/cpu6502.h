#pragma once

#include "alu.h"
#include "cpu.h"

#include <bitset>
#include <cstdint>

class Bus;

enum class CpuStep : uint8_t { T0, T1, T2, T3 };
enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };

class CPU6502 : public CPU {
public:
    explicit CPU6502(Bus &bus);

    void reset() override;
    void executeInstruction() override;
    void tick();

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
    uint8_t m_IR = 0;

    // The ALU is a combinational unit: it has no state of its own, but real
    // hardware wires its inputs and output through latches rather than
    // passing them as ephemeral call arguments. These members are that
    // wiring, made explicit.
    ALU m_alu;
    uint8_t m_aluA = 0;
    uint8_t m_aluB = 0;
    bool m_aluCarryIn = false;
    AluResult m_aluOutput{};

private:
    void onClockHigh();
    void onClockLow();
    void captureOpcodeFetch();
    void commitOpcodeFetch();
    void tickAlu();
    void applyAdc(uint8_t operand);
    void tickADCImmediate();
    void tickADCAbsolute();
};
