#include "tty.h"

uint8_t TTY::read(uint16_t offset) const {
    if (offset <= 1) {
        return readSerial(offset);
    }
    if (offset >= 2 && offset <= 4) {
        return readTape(offset);
    }
    return 0x00;
}

void TTY::write(uint16_t offset, uint8_t val) {
    if (offset <= 1) {
        writeSerial(offset, val);
        return;
    }
    if (offset >= 2 && offset <= 4) {
        writeTape(offset, val);
    }
    // every other offset in the page: reserved, writes ignored
}

uint8_t TTY::readSerial(uint16_t offset) const {
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

void TTY::writeSerial(uint16_t offset, uint8_t val) {
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

uint8_t TTY::readTape(uint16_t offset) const {
    if (offset == 0x02) {
        uint8_t status = 0;
        if (m_tapeBacking != nullptr) {
            status |= 0x01; // PRESENT
        }
        if (m_tapeMotorOn) {
            status |= 0x02; // MOTOR
        }
        if (m_tapeEot) {
            status |= 0x04; // EOT
        }
        if (m_tapeError) {
            status |= 0x08; // ERROR
        }
        return status;
    }
    if (offset == 0x03) {
        return 0x00; // TAPE_CONTROL is write-only
    }
    // offset == 0x04: TAPE_DATA
    if (m_tapeBacking == nullptr || !m_tapeMotorOn) {
        m_tapeError = true;
        return 0x00;
    }
    m_tapeBacking->seekg(static_cast<std::streamoff>(m_tapePosition));
    int c = m_tapeBacking->get();
    if (c == std::char_traits<char>::eof()) {
        m_tapeEot = true;
        m_tapeError = true;
        m_tapeBacking->clear();
        return 0x00;
    }
    m_tapeEot = false;
    m_tapeError = false;
    ++m_tapePosition;
    return static_cast<uint8_t>(c);
}

void TTY::writeTape(uint16_t offset, uint8_t val) {
    if (offset == 0x02) {
        return; // TAPE_STATUS is read-only
    }
    if (offset == 0x03) {
        m_tapeMotorOn = (val & 0x01) != 0;
        if ((val & 0x02) != 0) {
            m_tapePosition = 0;
            m_tapeEot = false;
            m_tapeError = false;
        }
        return;
    }
    // offset == 0x04: TAPE_DATA
    if (m_tapeBacking == nullptr || !m_tapeMotorOn) {
        m_tapeError = true;
        return;
    }
    m_tapeBacking->seekp(static_cast<std::streamoff>(m_tapePosition));
    m_tapeBacking->put(static_cast<char>(val));
    m_tapeBacking->flush();
    ++m_tapePosition;
    m_tapeEot = false;
    m_tapeError = false;
}
