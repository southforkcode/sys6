#pragma once

#include "memory_device.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class RAM : public MemoryDevice {
public:
    explicit RAM(size_t size);

    size_t size() const override;
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

private:
    std::vector<uint8_t> m_data;
};
