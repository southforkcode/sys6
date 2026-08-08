#include "ram.h"

RAM::RAM(size_t size) : m_data(size, 0) {}

size_t RAM::size() const { return m_data.size(); }

uint8_t RAM::read(uint16_t offset) const { return m_data[offset]; }

void RAM::write(uint16_t offset, uint8_t val) { m_data[offset] = val; }
