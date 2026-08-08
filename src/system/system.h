#pragma once

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "peripherals/tty.h"
#include "system/terminal_io.h"

#include <ostream>

class System {
public:
    System(TerminalIO &term, std::ostream &out);

    void reset(); // resets the CPU -- exposed so tests can drive step()
                   // from a known state without going through run()'s
                   // infinite loop
    void run();    // reset() once, then loops step() forever
    void step();   // one read/inject/execute iteration

private:
    TerminalIO &m_term;
    RAM m_ram;
    TTY m_tty;
    ROM m_rom;
    Bus m_bus;
    CPU6502 m_cpu;
};
