#include "alu.h"

AluResult ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const {
    switch (function) {
    case AluFunction::ADD: {
        int sum = static_cast<int>(a) + static_cast<int>(b) + (carryIn ? 1 : 0);
        return AluResult{static_cast<uint8_t>(sum & 0xFF), sum > 0xFF};
    }
    case AluFunction::AND:
        return AluResult{static_cast<uint8_t>(a & b), false};
    case AluFunction::OR:
        return AluResult{static_cast<uint8_t>(a | b), false};
    case AluFunction::XOR:
        return AluResult{static_cast<uint8_t>(a ^ b), false};
    case AluFunction::SHL:
        return AluResult{static_cast<uint8_t>(b << 1), (b & 0x80) != 0};
    case AluFunction::SHR:
        return AluResult{static_cast<uint8_t>(b >> 1), (b & 0x01) != 0};
    case AluFunction::ROL:
        return AluResult{static_cast<uint8_t>((b << 1) | (carryIn ? 0x01 : 0x00)), (b & 0x80) != 0};
    case AluFunction::ROR:
        return AluResult{static_cast<uint8_t>((b >> 1) | (carryIn ? 0x80 : 0x00)), (b & 0x01) != 0};
    }
    return AluResult{0, false}; // unreachable: all enumerators handled above
}
