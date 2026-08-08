#include "alu.h"

namespace {
AluResult fromValue(uint8_t value, bool carry, bool overflow) {
    AluResult result;
    result.value = value;
    result.carry = carry;
    result.overflow = overflow;
    result.zero = value == 0;
    result.negative = (value & 0x80) != 0;
    return result;
}
} // namespace

AluResult ALU::adc(uint8_t acc, uint8_t operand, bool carryIn) const {
    int sum = static_cast<int>(acc) + static_cast<int>(operand) + (carryIn ? 1 : 0);
    auto value = static_cast<uint8_t>(sum & 0xFF);
    bool overflow = ((~(acc ^ operand)) & (acc ^ value) & 0x80) != 0;
    return fromValue(value, sum > 0xFF, overflow);
}

AluResult ALU::sbc(uint8_t acc, uint8_t operand, bool carryIn) const {
    // Subtract is add-with-inverted-operand on real 6502 hardware: SBC's
    // carryIn doubles as "not borrow," making this identity exact.
    return adc(acc, static_cast<uint8_t>(~operand), carryIn);
}

AluResult ALU::bitwiseAnd(uint8_t acc, uint8_t operand) const {
    return fromValue(static_cast<uint8_t>(acc & operand), false, false);
}

AluResult ALU::bitwiseOr(uint8_t acc, uint8_t operand) const {
    return fromValue(static_cast<uint8_t>(acc | operand), false, false);
}

AluResult ALU::bitwiseXor(uint8_t acc, uint8_t operand) const {
    return fromValue(static_cast<uint8_t>(acc ^ operand), false, false);
}

AluResult ALU::cmp(uint8_t reg, uint8_t operand) const {
    // A compare is a subtract with the result discarded and no incoming
    // borrow; reuses the same add-with-inverted-operand identity as sbc.
    return adc(reg, static_cast<uint8_t>(~operand), true);
}

AluResult ALU::asl(uint8_t value) const {
    return fromValue(static_cast<uint8_t>(value << 1), (value & 0x80) != 0, false);
}

AluResult ALU::lsr(uint8_t value) const {
    return fromValue(static_cast<uint8_t>(value >> 1), (value & 0x01) != 0, false);
}

AluResult ALU::rol(uint8_t value, bool carryIn) const {
    auto shifted = static_cast<uint8_t>((value << 1) | (carryIn ? 0x01 : 0x00));
    return fromValue(shifted, (value & 0x80) != 0, false);
}

AluResult ALU::ror(uint8_t value, bool carryIn) const {
    auto shifted = static_cast<uint8_t>((value >> 1) | (carryIn ? 0x80 : 0x00));
    return fromValue(shifted, (value & 0x01) != 0, false);
}

AluResult ALU::increment(uint8_t value) const {
    return fromValue(static_cast<uint8_t>(value + 1), false, false);
}

AluResult ALU::decrement(uint8_t value) const {
    return fromValue(static_cast<uint8_t>(value - 1), false, false);
}

AluResult ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const {
    switch (function) {
    case AluFunction::ADD: {
        int sum = static_cast<int>(a) + static_cast<int>(b) + (carryIn ? 1 : 0);
        return AluResult{static_cast<uint8_t>(sum & 0xFF), sum > 0xFF, false, false, false};
    }
    case AluFunction::AND:
        return AluResult{static_cast<uint8_t>(a & b), false, false, false, false};
    case AluFunction::OR:
        return AluResult{static_cast<uint8_t>(a | b), false, false, false, false};
    case AluFunction::XOR:
        return AluResult{static_cast<uint8_t>(a ^ b), false, false, false, false};
    case AluFunction::SHL:
        return AluResult{static_cast<uint8_t>(b << 1), (b & 0x80) != 0, false, false, false};
    case AluFunction::SHR:
        return AluResult{static_cast<uint8_t>(b >> 1), (b & 0x01) != 0, false, false, false};
    case AluFunction::ROL:
        return AluResult{static_cast<uint8_t>((b << 1) | (carryIn ? 0x01 : 0x00)), (b & 0x80) != 0, false, false,
                          false};
    case AluFunction::ROR:
        return AluResult{static_cast<uint8_t>((b >> 1) | (carryIn ? 0x80 : 0x00)), (b & 0x01) != 0, false, false,
                          false};
    }
    return AluResult{0, false, false, false, false}; // unreachable: all enumerators handled above
}
