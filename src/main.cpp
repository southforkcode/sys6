#include <iostream>

#include "utils/log.h"
#include "cpu/cpu6502.h"

int main(int argc, char* argv[]) {
    Logger logger(std::cout, Logger::log_level_t::DEBUG);
    logger.info("Starting CPU6502 emulator...");
    CPU6502 cpu;
    cpu.setLogger(&logger);
    cpu.Tracing(true);
    cpu.Debug(false);
    cpu.reset();
    cpu.executeInstruction();
    logger.info("CPU6502 emulator finished.");
    return 0;
}