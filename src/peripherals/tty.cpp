#include "tty.h"

uint8_t TTY::read(uint16_t offset) const {
    if (offset == 0) {
        uint8_t status = 0x02; // TXRDY: always ready, no output buffering to model
        if (m_rxReady) {
            status |= 0x01;
        }
        return status;
    }
    // offset == 1: DATA
    m_rxReady = false;
    return m_rxByte;
}

void TTY::write(uint16_t offset, uint8_t val) {
    if (offset == 1) {
        m_out.put(static_cast<char>(val));
        m_out.flush();
    }
}

void TTY::receive(uint8_t byte) {
    if (m_rxReady) {
        return; // single holding register, no FIFO
    }
    m_rxByte = byte;
    m_rxReady = true;
}
