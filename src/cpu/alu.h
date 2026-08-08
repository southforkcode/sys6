#pragma once

#include <cstdint>

struct AluResult {
    uint8_t value;
    bool carry;
    bool zero;
    bool overflow;
    bool negative;
};

enum class AluFunction : uint8_t { ADD, AND, OR, XOR, SHL, SHR, ROL, ROR };

class ALU {
public:
    [[nodiscard]] AluResult adc(uint8_t acc, uint8_t operand, bool carryIn) const;
    [[nodiscard]] AluResult sbc(uint8_t acc, uint8_t operand, bool carryIn) const;
    [[nodiscard]] AluResult bitwiseAnd(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult bitwiseOr(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult bitwiseXor(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult cmp(uint8_t reg, uint8_t operand) const;
    [[nodiscard]] AluResult asl(uint8_t value) const;
    [[nodiscard]] AluResult lsr(uint8_t value) const;
    [[nodiscard]] AluResult rol(uint8_t value, bool carryIn) const;
    [[nodiscard]] AluResult ror(uint8_t value, bool carryIn) const;
    [[nodiscard]] AluResult increment(uint8_t value) const;
    [[nodiscard]] AluResult decrement(uint8_t value) const;

    [[nodiscard]] AluResult execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const;
};
