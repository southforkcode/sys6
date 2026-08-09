#include "system.h"

#include "system/monitor_firmware.h"

namespace {
constexpr uint16_t kRamSize = 0x8000;
} // namespace

System::System(TerminalIO &term, std::ostream &out, std::iostream *tapeBacking)
    : m_term(term), m_ram(kRamSize), m_tty(out, tapeBacking), m_rom(std::vector<uint8_t>(monitor::kRomSize)),
      m_cpu(m_bus) {
    m_bus.attach(0x0000, 0x7FFF, m_ram);
    m_bus.attach(0x8000, 0x80FF, m_tty);
    m_bus.attach(0xC000, 0xFFFF, m_rom);
    monitor::install(m_rom);
}

void System::reset() { m_cpu.reset(); }

void System::step() {
    // Only pull a new byte when TTY's single holding register is free --
    // otherwise a byte read from the real terminal (or a fake's queue)
    // would be silently dropped by TTY::receive() before the CPU ever
    // gets a chance to consume the one already latched.
    if (!m_tty.rxReady()) {
        if (auto b = m_term.tryReadByte()) {
            m_tty.receive(*b);
        }
    }
    m_cpu.executeInstruction();
}

void System::run() {
    reset();
    while (true) {
        step();
    }
}
