#include <gtest/gtest.h>

#include "fake_terminal_io.h"
#include "system/system.h"

#include <sstream>

TEST(SystemTest, StepInjectsAvailableByteAndAdvancesTheFirmware) {
    FakeTerminalIO term;
    std::ostringstream out;
    System system(term, out);
    system.reset();
    // "0300", not "0000" -- LINEBUF lives at RAM $00-$3F, so typing "0000"
    // would store '0' (0x30) into LINEBUF[0], which *is* address $0000,
    // and the peek would show that instead of fresh RAM.
    for (char c : std::string("0300")) {
        term.push(static_cast<uint8_t>(c));
    }
    term.push(0x0D);
    for (int i = 0; i < 20000; ++i) {
        system.step();
    }
    EXPECT_EQ(out.str(), "sys6 monitor\r\n> 0300\r\n0300: 00\r\n> ");
}

TEST(SystemTest, StepDoesNotFabricateInputWhenNoneIsAvailable) {
    FakeTerminalIO term;
    std::ostringstream out;
    System system(term, out);
    system.reset();
    for (int i = 0; i < 2000; ++i) {
        system.step();
    }
    EXPECT_EQ(out.str(), "sys6 monitor\r\n> ");
}
