#pragma once

#include "cpu/cpu6502.h"
#include "peripherals/tty.h"

#include <cstdint>
#include <string>

// Delivers one byte through the TTY the way a real keystroke would: injects
// it, then pumps the CPU until GETCHAR has actually consumed it (rxReady()
// goes back to false), bounded so a firmware bug shows up as a test
// failure instead of a hang.
inline void typeChar(CPU6502 &cpu, TTY &tty, uint8_t byte) {
    tty.receive(byte);
    for (int i = 0; i < 2000 && tty.rxReady(); ++i) {
        cpu.executeInstruction();
    }
}

// Types every character of `line`, then a trailing CR, then pumps a
// generous fixed instruction budget so the firmware finishes dispatching
// the command and settles back at GETCHAR's wait loop before returning.
inline void typeLine(CPU6502 &cpu, TTY &tty, const std::string &line) {
    for (char c : line) {
        typeChar(cpu, tty, static_cast<uint8_t>(c));
    }
    typeChar(cpu, tty, 0x0D);
    for (int i = 0; i < 20000; ++i) {
        cpu.executeInstruction();
    }
}
