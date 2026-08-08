#pragma once

#include <cstdint>

struct AluResult {
    uint8_t value;
    bool carry;
    bool zero;
    bool overflow;
    bool negative;
};

class ALU {
public:
    [[nodiscard]] AluResult adc(uint8_t acc, uint8_t operand, bool carryIn) const;
};
