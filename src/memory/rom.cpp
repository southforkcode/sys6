#include "rom.h"

#include <string>

ROM::ROM(std::vector<uint8_t> data) : m_data(std::move(data)) {}

void ROM::setLogger(Logger *logger) { m_logger = logger; }

size_t ROM::size() const { return m_data.size(); }

uint8_t ROM::read(uint16_t offset) const { return m_data[offset]; }

void ROM::write(uint16_t offset, uint8_t val) {
    (void)val;
    if (m_logger) {
        m_logger->warn("Ignored write to ROM at offset " + std::to_string(offset));
    }
}
