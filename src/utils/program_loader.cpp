#include "program_loader.h"

#include <cctype>
#include <stdexcept>

namespace {
int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    throw std::invalid_argument("loadProgram: invalid hex character");
}
} // namespace

void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex) {
    std::string digits;
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        }
    }

    if (digits.size() % 2 != 0) {
        throw std::invalid_argument("loadProgram: hex string has odd digit count");
    }

    uint16_t address = startAddress;
    for (size_t i = 0; i < digits.size(); i += 2) {
        auto byte = static_cast<uint8_t>((hexDigitValue(digits[i]) << 4) | hexDigitValue(digits[i + 1]));
        device.write(address, byte);
        address = static_cast<uint16_t>(address + 1);
    }
}
