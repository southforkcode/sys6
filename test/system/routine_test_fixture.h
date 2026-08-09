#pragma once

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "peripherals/tty.h"
#include "system/monitor_firmware.h"

#include <sstream>
#include <string>
#include <vector>

// Builds a RAM+TTY+ROM+Bus+CPU6502 for testing individual monitor routines
// in isolation, the same way cpu6502_fibonacci_e2e_test.cpp tests a whole
// program: load a tiny hand-assembled "driver" snippet that sets up
// inputs, JSRs into the routine(s) under test, and ends in BRK; point the
// reset vector at it; run via cpu.run()/cpu.halted().
struct RoutineTestFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty;
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    explicit RoutineTestFixture(std::iostream *tapeBacking = nullptr) : tty(output, tapeBacking) {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x80FF, tty);
        bus.attach(0xC000, 0xFFFF, rom);
    }

    // Loads `hex` at the reserved test-driver page and points the reset
    // vector at it. Call cpu.reset() after this to boot into the driver.
    void loadDriver(const std::string &hex) {
        monitor::loadRoutine(rom, monitor::kTestDriverAddr, hex);
        monitor::loadRoutine(rom, 0xFFFC, "00 C9"); // reset vector -> $C900
    }
};
