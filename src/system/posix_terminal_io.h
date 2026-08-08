#pragma once

#include "system/terminal_io.h"

#include <termios.h>

// Puts real stdin into raw mode (no OS echo, no line buffering, non-
// blocking reads) for the duration of its lifetime, and restores the
// original settings on destruction -- RAII, so terminal state is always
// cleaned up on the way out, including on exceptions.
class PosixTerminalIO : public TerminalIO {
public:
    PosixTerminalIO();
    ~PosixTerminalIO() override;

    PosixTerminalIO(const PosixTerminalIO &) = delete;
    PosixTerminalIO &operator=(const PosixTerminalIO &) = delete;

    std::optional<uint8_t> tryReadByte() override;

private:
    struct termios m_savedTermios {};
};
