#pragma once

#include <cstdint>

struct AluResult {
    uint8_t value; // D
    bool carry;    // CO
};

enum class AluFunction : uint8_t { ADD, AND, OR, XOR, SHL, SHR, ROL, ROR };

class ALU {
public:
    [[nodiscard]] AluResult execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const;
};
