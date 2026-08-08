#pragma once

#include <cstdint>
#include <optional>

// Input-only seam between System and the real OS. Output doesn't need one
// -- TTY already takes a plain std::ostream&, so a test can capture output
// with an std::ostringstream independent of however input is faked here.
class TerminalIO {
public:
    virtual ~TerminalIO() = default;
    virtual std::optional<uint8_t> tryReadByte() = 0;
};
