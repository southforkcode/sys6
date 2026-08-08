#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <ostream>

class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out) : m_out(out) {}

    size_t size() const override { return 2; }
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API (not part of MemoryDevice) -- called by System, or
    // directly by tests in place of a real terminal.
    void receive(uint8_t byte);
    bool rxReady() const { return m_rxReady; }

private:
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;
};
