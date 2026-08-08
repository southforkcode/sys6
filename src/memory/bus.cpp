#include "bus.h"

#include <stdexcept>
#include <string>

void Bus::attach(uint16_t start, uint16_t end, MemoryDevice &device) {
    if (end < start) {
        throw std::invalid_argument("Bus::attach: end address is before start address");
    }

    size_t rangeSize = static_cast<size_t>(end) - static_cast<size_t>(start) + 1;
    if (rangeSize != device.size()) {
        throw std::invalid_argument("Bus::attach: range size does not match device size");
    }

    for (const auto &mapping : m_mappings) {
        if (start <= mapping.end && end >= mapping.start) {
            throw std::invalid_argument("Bus::attach: range overlaps an existing mapping");
        }
    }

    m_mappings.push_back({start, end, &device});
}

const Bus::Mapping *Bus::findMapping(uint16_t addr) const {
    for (const auto &mapping : m_mappings) {
        if (addr >= mapping.start && addr <= mapping.end) {
            return &mapping;
        }
    }
    return nullptr;
}

uint8_t Bus::read(uint16_t addr) const {
    const Mapping *mapping = findMapping(addr);
    if (!mapping) {
        if (m_logger) {
            m_logger->warn("Bus::read: no device mapped at address " + std::to_string(addr));
        }
        return 0xFF;
    }
    return mapping->device->read(addr - mapping->start);
}

void Bus::write(uint16_t addr, uint8_t val) {
    const Mapping *mapping = findMapping(addr);
    if (!mapping) {
        if (m_logger) {
            m_logger->warn("Bus::write: no device mapped at address " + std::to_string(addr));
        }
        return;
    }
    mapping->device->write(addr - mapping->start, val);
}

void Bus::setLogger(Logger *logger) { m_logger = logger; }
