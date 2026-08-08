#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <string>

void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex);
