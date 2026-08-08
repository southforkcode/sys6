#include <gtest/gtest.h>

#include "routine_test_fixture.h"

TEST(MonitorRoutines, PutCharWritesToOutput) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 41"    // LDA #$41           ('A')
                  "20 00 C1" // JSR PUTCHAR
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "A");
}

TEST(MonitorRoutines, GetCharReturnsAndConsumesReceivedByte) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kGetCharAddr, monitor::kGetCharHex);
    fx.loadDriver("20 00 C0" // JSR GETCHAR
                  "85 50"    // STA $50            (stash received char)
                  "00");     // BRK
    fx.tty.receive('X');
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 'X');
    EXPECT_FALSE(fx.tty.rxReady());
}

TEST(MonitorRoutines, PrintHexByteFormatsHighAndLowNibbles) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPrintHexByteAddr, monitor::kPrintHexByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintNibbleAddr, monitor::kPrintNibbleHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 3F"    // LDA #$3F
                  "20 00 C2" // JSR PRINT_HEX_BYTE
                  "A9 A0"    // LDA #$A0
                  "20 00 C2" // JSR PRINT_HEX_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "3FA0");
}

TEST(MonitorRoutines, PrintStringPrintsUntilNull) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPrintStringAddr, monitor::kPrintStringHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 0C"      // LDA #$0C           (string is 12 bytes past driver start)
                  "85 FC"      // STA $FC
                  "A9 C9"      // LDA #$C9
                  "85 FD"      // STA $FD
                  "20 00 C3"   // JSR PRINT_STRING
                  "00"         // BRK
                  "48 69 00"); // "Hi\0"
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "Hi");
}

TEST(MonitorRoutines, HexValAcceptsDigitsUpperAndLowerLetters) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    fx.loadDriver("A9 39"    // LDA #$39           ('9')
                  "20 00 C7" // JSR HEXVAL
                  "85 50"    // STA $50            (expect 9)
                  "A9 42"    // LDA #$42           ('B')
                  "20 00 C7" // JSR HEXVAL
                  "85 51"    // STA $51            (expect 11)
                  "A9 66"    // LDA #$66           ('f')
                  "20 00 C7" // JSR HEXVAL
                  "85 52"    // STA $52            (expect 15)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 9);
    EXPECT_EQ(fx.ram.read(0x51), 11);
    EXPECT_EQ(fx.ram.read(0x52), 15);
}

TEST(MonitorRoutines, HexValRejectsNonHexAndSetsCarry) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    fx.loadDriver("A9 47"    // LDA #$47           ('G' -- not hex)
                  "20 00 C7" // JSR HEXVAL
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01           (isolate carry bit)
                  "85 50"    // STA $50            (expect 1: invalid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 1);
}

TEST(MonitorRoutines, ParseAddrParsesFourHexDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 04"    // LDA #$04
                  "85 40"    // STA $40            (LINELEN = 4)
                  "A9 41"    // LDA #$41           ('A')
                  "85 00"    // STA $00
                  "A9 42"    // LDA #$42           ('B')
                  "85 01"    // STA $01
                  "A9 43"    // LDA #$43           ('C')
                  "85 02"    // STA $02
                  "A9 44"    // LDA #$44           ('D')
                  "85 03"    // STA $03
                  "20 00 C5" // JSR PARSE_ADDR
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01
                  "85 50"    // STA $50            (expect 0: valid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kAddrLoAddr), 0xCD);
    EXPECT_EQ(fx.ram.read(monitor::kAddrHiAddr), 0xAB);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 4);
    EXPECT_EQ(fx.ram.read(0x50), 0);
}

TEST(MonitorRoutines, ParseAddrStopsAtNonHexAndZeroExtends) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 35"    // LDA #$35           ('5')
                  "85 00"    // STA $00
                  "A9 3A"    // LDA #$3A           (':')
                  "85 01"    // STA $01
                  "20 00 C5" // JSR PARSE_ADDR
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kAddrLoAddr), 0x05);
    EXPECT_EQ(fx.ram.read(monitor::kAddrHiAddr), 0x00);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 1); // stopped before ':'
}

TEST(MonitorRoutines, ParseAddrFailsOnZeroDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 01"    // LDA #$01
                  "85 40"    // STA $40            (LINELEN = 1)
                  "A9 3A"    // LDA #$3A           (':')
                  "85 00"    // STA $00
                  "20 00 C5" // JSR PARSE_ADDR
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01
                  "85 50"    // STA $50            (expect 1: invalid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(0x50), 1);
}

TEST(MonitorRoutines, ParseByteParsesTwoDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 39"    // LDA #$39           ('9')
                  "85 00"    // STA $00
                  "A9 46"    // LDA #$46           ('F')
                  "85 01"    // STA $01
                  "20 00 C6" // JSR PARSE_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kByteValAddr), 0x9F);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 2);
}

TEST(MonitorRoutines, ParseByteStopsAfterOneDigitOnNonHex) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 37"    // LDA #$37           ('7')
                  "85 00"    // STA $00
                  "A9 20"    // LDA #$20           (' ')
                  "85 01"    // STA $01
                  "20 00 C6" // JSR PARSE_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kByteValAddr), 0x07);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 1); // stopped at the space
}
