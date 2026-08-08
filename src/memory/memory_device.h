#pragma once

#include <cstddef>
#include <cstdint>

class MemoryDevice {
public:
    virtual ~MemoryDevice() = default;

    virtual size_t size() const = 0;
    virtual uint8_t read(uint16_t offset) const = 0;
    virtual void write(uint16_t offset, uint8_t val) = 0;
};
