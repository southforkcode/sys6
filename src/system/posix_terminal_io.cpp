#include "posix_terminal_io.h"

#include <unistd.h>

PosixTerminalIO::PosixTerminalIO() {
    tcgetattr(STDIN_FILENO, &m_savedTermios);
    struct termios raw = m_savedTermios;
    // cfmakeraw(), not a hand-rolled subset: a partial flip of just
    // ICANON/ECHO/OPOST leaves ICRNL set, which silently remaps an
    // incoming '\r' (what Enter actually sends) to '\n' before this
    // process ever reads it -- and READ_LINE specifically checks for
    // 0x0D, so it would never recognize end-of-line.
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

PosixTerminalIO::~PosixTerminalIO() { tcsetattr(STDIN_FILENO, TCSANOW, &m_savedTermios); }

std::optional<uint8_t> PosixTerminalIO::tryReadByte() {
    unsigned char byte = 0;
    ssize_t n = read(STDIN_FILENO, &byte, 1);
    if (n == 1) {
        return byte;
    }
    return std::nullopt;
}
