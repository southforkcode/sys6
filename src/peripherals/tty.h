#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <iostream>

class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out, std::iostream *tapeBacking = nullptr)
        : m_out(out), m_tapeBacking(tapeBacking) {}

    size_t size() const override { return 256; }
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API (not part of MemoryDevice) -- called by System, or
    // directly by tests in place of a real terminal.
    void receive(uint8_t byte);
    bool rxReady() const { return m_rxReady; }

private:
    uint8_t readSerial(uint16_t offset) const;
    void writeSerial(uint16_t offset, uint8_t val);
    uint8_t readTape(uint16_t offset) const;
    void writeTape(uint16_t offset, uint8_t val);

    // serial (offsets 0x00/0x01)
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;

    // tape (offsets 0x02-0x04) -- a single m_tapePosition cursor drives
    // both reads and writes, modeling a real tape's one physical head
    // rather than independent read/write pointers that could diverge.
    std::iostream *m_tapeBacking;
    mutable size_t m_tapePosition = 0;
    bool m_tapeMotorOn = false;
    mutable bool m_tapeEot = false;
    mutable bool m_tapeError = false;
};
