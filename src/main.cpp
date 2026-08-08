#include <iostream>
#include <vector>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "utils/log.h"

int main(int argc, char *argv[]) {
    Logger logger(std::cout, Logger::log_level_t::DEBUG);
    logger.info("Starting CPU6502 emulator...");

    RAM ram(0x8000);                       // 0x0000-0x7FFF
    ROM rom(std::vector<uint8_t>(0x8000)); // 0x8000-0xFFFF, placeholder zeroed data

    Bus bus;
    bus.setLogger(&logger);
    bus.attach(0x0000, 0x7FFF, ram);
    bus.attach(0x8000, 0xFFFF, rom);

    CPU6502 cpu(bus);
    cpu.setLogger(&logger);
    cpu.Tracing(true);
    cpu.Debug(false);
    cpu.reset();
    cpu.executeInstruction();
    logger.info("CPU6502 emulator finished.");
    return 0;
}
