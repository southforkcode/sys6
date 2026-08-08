#pragma once

#include "memory_device.h"
#include "utils/log.h"

#include <cstdint>
#include <vector>

class Bus {
public:
    void attach(uint16_t start, uint16_t end, MemoryDevice &device);
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t val);
    void setLogger(Logger *logger);

private:
    struct Mapping {
        uint16_t start;
        uint16_t end;
        MemoryDevice *device;
    };

    const Mapping *findMapping(uint16_t addr) const;

    std::vector<Mapping> m_mappings;
    Logger *m_logger = nullptr;
};
