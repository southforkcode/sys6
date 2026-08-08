#include "alu.h"

AluResult ALU::adc(uint8_t acc, uint8_t operand, bool carryIn) const {
    int sum = static_cast<int>(acc) + static_cast<int>(operand) + (carryIn ? 1 : 0);
    uint8_t value = static_cast<uint8_t>(sum & 0xFF);

    AluResult result;
    result.value = value;
    result.carry = sum > 0xFF;
    result.overflow = ((~(acc ^ operand)) & (acc ^ value) & 0x80) != 0;
    result.zero = value == 0;
    result.negative = (value & 0x80) != 0;
    return result;
}
