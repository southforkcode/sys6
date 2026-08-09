#include <gtest/gtest.h>

#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "monitor_test_helpers.h"
#include "peripherals/tty.h"
#include "system/monitor_firmware.h"

#include <sstream>
#include <vector>

namespace {
struct MonitorFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty;
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    explicit MonitorFixture(std::iostream *tapeBacking = nullptr) : tty(output, tapeBacking) {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x80FF, tty);
        bus.attach(0xC000, 0xFFFF, rom);
        monitor::install(rom);
    }
};

constexpr const char *kBannerAndPrompt = "sys6 monitor\r\n> ";
} // namespace

TEST(MonitorFirmwareE2E, ColdStartPrintsBannerThenPrompt) {
    MonitorFixture fx;
    fx.cpu.reset();
    for (int i = 0; i < 5000; ++i) {
        fx.cpu.executeInstruction();
    }
    EXPECT_EQ(fx.output.str(), kBannerAndPrompt);
}

TEST(MonitorFirmwareE2E, EchoAppearsBeforeCommandOutput) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050");
    std::string expected = kBannerAndPrompt;
    expected += "0050\r\n0050: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, PeekReturnsKnownByte) {
    MonitorFixture fx;
    fx.ram.write(0x0300, 0x7E);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    expected += "0300\r\n0300: 7E\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, PokeThenPeekRoundTrips) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0400: 11 22");
    typeLine(fx.cpu, fx.tty, "0400");
    std::string expected = kBannerAndPrompt;
    expected += "0400: 11 22\r\n> 0400\r\n0400: 11\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, ListSpansMultipleRows) {
    MonitorFixture fx;
    for (int i = 0; i < 18; ++i) {
        fx.ram.write(static_cast<uint16_t>(0x0500 + i), static_cast<uint8_t>(i));
    }
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0500.0511");
    std::string expected = kBannerAndPrompt;
    expected += "0500.0511\r\n"
                 "0500: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \r\n"
                 "0510: 10 11 \r\n"
                 "> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, RunViaRtsExecutesProgramAndReturnsToPrompt) {
    MonitorFixture fx;
    // INC $80 ; RTS
    fx.ram.write(0x0600, 0xE6);
    fx.ram.write(0x0601, 0x80);
    fx.ram.write(0x0602, 0x60);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0600 R");
    typeLine(fx.cpu, fx.tty, "0080");
    std::string expected = kBannerAndPrompt;
    expected += "0600 R\r\n> 0080\r\n0080: 01\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, RunViaBrkReachesWarmStartPromptNotBanner) {
    MonitorFixture fx;
    fx.ram.write(0x0700, 0x00); // BRK
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0700 R");
    // Peek an address outside LINEBUF ($00-$3F) -- "0000" would collide
    // with the very line just typed to reach this prompt.
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    // No second banner after the BRK -- just the warm-start prompt -- and
    // the monitor is still usable afterward.
    expected += "0700 R\r\n> 0300\r\n0300: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, BackspaceCorrectsTypedAddress) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, '5');
    typeChar(fx.cpu, fx.tty, 0x7F); // backspace: erases the '5'
    typeChar(fx.cpu, fx.tty, '5');
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, 0x0D); // CR: dispatches "0050"
    for (int i = 0; i < 20000; ++i) {
        fx.cpu.executeInstruction();
    }
    std::string expected = kBannerAndPrompt;
    expected += "005";
    expected += "\x08 \x08"; // visual erase of the typo'd '5'
    expected += "50\r\n0050: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, MalformedLineThenValidCommandRecovers) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "ZZ");
    // Peek an address outside LINEBUF ($00-$3F) -- see the comment in
    // RunViaBrkReachesWarmStartPromptNotBanner above.
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    expected += "ZZ\r\n?\r\n> 0300\r\n0300: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, MultiCommandSessionChainsCleanly) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0200: AA BB CC");
    typeLine(fx.cpu, fx.tty, "0200");
    typeLine(fx.cpu, fx.tty, "0200.0202");
    // DEC $80 ; RTS
    fx.ram.write(0x0210, 0xC6);
    fx.ram.write(0x0211, 0x80);
    fx.ram.write(0x0212, 0x60);
    fx.ram.write(0x0080, 0x05);
    typeLine(fx.cpu, fx.tty, "0210 R");
    typeLine(fx.cpu, fx.tty, "0080");
    std::string expected = kBannerAndPrompt;
    expected += "0200: AA BB CC\r\n> "
                 "0200\r\n0200: AA\r\n> "
                 "0200.0202\r\n0200: AA BB CC \r\n> "
                 "0210 R\r\n> "
                 "0080\r\n0080: 04\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, AddressEdgeCasesParseCorrectly) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "5");    // 1-digit, zero-extends to $0005
    typeLine(fx.cpu, fx.tty, "FFFF"); // top of the address space: BRK-vector hi byte
    std::string expected = kBannerAndPrompt;
    expected += "5\r\n0005: 00\r\n> FFFF\r\nFFFF: CF\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, SaveRewindLoadRoundTrip) {
    std::stringstream tape;
    MonitorFixture fx(&tape);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.ram.write(0x0052, 0xCC);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0052 S");
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    typeLine(fx.cpu, fx.tty, "0070");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0052 S\r\n> ";
    expected += "0 W\r\n> ";
    expected += "0070 L\r\n> ";
    expected += "0070\r\n0070: AA\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
}

TEST(MonitorFirmwareE2E, LoadWithCorruptedChecksumStillLandsPartialDataAndReportsError) {
    std::stringstream tape;
    MonitorFixture fx(&tape);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0051 S");
    std::string saved = tape.str();
    ASSERT_EQ(saved.size(), 5u); // LEN(2) + 2 data bytes + checksum
    saved.back() = static_cast<char>(saved.back() ^ 0xFF); // corrupt the checksum
    tape.str(saved);
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0051 S\r\n> ";
    expected += "0 W\r\n> ";
    expected += "0070 L\r\n?\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
}

TEST(MonitorFirmwareE2E, TapeCommandsFailCleanlyWithNoTapeAttachedAndPromptStaysUsable) {
    MonitorFixture fx; // no tape backing
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0051 S");
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0051 S\r\n?\r\n> ";
    expected += "0 W\r\n?\r\n> ";
    expected += "0070 L\r\n?\r\n> ";
    expected += "0300\r\n0300: 00\r\n> "; // prompt is still fully usable afterward
    EXPECT_EQ(fx.output.str(), expected);
}
