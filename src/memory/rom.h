#pragma once

#include "memory_device.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class ROM : public MemoryDevice {
public:
    explicit ROM(std::vector<uint8_t> data);

    void setLogger(Logger *logger);

    size_t size() const override;
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

private:
    std::vector<uint8_t> m_data;
    Logger *m_logger = nullptr;
};
