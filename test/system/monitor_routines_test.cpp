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

namespace {
void loadDispatchDeps(RoutineTestFixture &fx) {
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintHexByteAddr, monitor::kPrintHexByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintNibbleAddr, monitor::kPrintNibbleHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kDispatchAddr, monitor::kDispatchHex);
    monitor::loadRoutine(fx.rom, monitor::kPeekAddr, monitor::kPeekHex);
    monitor::loadRoutine(fx.rom, monitor::kListAddr, monitor::kListHex);
    monitor::loadRoutine(fx.rom, monitor::kPokeAddr, monitor::kPokeHex);
    monitor::loadRoutine(fx.rom, monitor::kRunAddr, monitor::kRunHex);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
}
} // namespace

TEST(MonitorRoutines, DispatchPeeksWhenLineEndsAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0xAB);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 40"    // STA $40            (LINELEN = 0)
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0            (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "0050: AB\r\n");
}

TEST(MonitorRoutines, DispatchListsRangeAcrossOneRow) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0x11);
    fx.ram.write(0x0051, 0x22);
    fx.ram.write(0x0052, 0x33);
    // LINEBUF = ".0052", LINEPOS = 0, LINELEN = 5; $F0/$F1 already holds
    // the first address ($0050), as if MAIN_LOOP's PARSE_ADDR had already
    // run -- DISPATCH picks up from there.
    fx.loadDriver("A9 2E"    // LDA #$2E           ('.')
                  "85 00"    // STA $00
                  "A9 30"    // LDA #$30           ('0')
                  "85 01"    // STA $01
                  "A9 30"    // LDA #$30           ('0')
                  "85 02"    // STA $02
                  "A9 35"    // LDA #$35           ('5')
                  "85 03"    // STA $03
                  "A9 32"    // LDA #$32           ('2')
                  "85 04"    // STA $04
                  "A9 05"    // LDA #$05
                  "85 40"    // STA $40            (LINELEN = 5)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0            (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.output.str(), "0050: 11 22 33 \r\n");
}

TEST(MonitorRoutines, DispatchPokesBytesStartingAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = ": AB CD", LINEPOS = 0, LINELEN = 7; $F0/$F1 = $0060.
    fx.loadDriver("A9 3A"    // LDA #$3A           (':')
                  "85 00"    // STA $00
                  "A9 20"    // LDA #$20           (' ')
                  "85 01"    // STA $01
                  "A9 41"    // LDA #$41           ('A')
                  "85 02"    // STA $02
                  "A9 42"    // LDA #$42           ('B')
                  "85 03"    // STA $03
                  "A9 20"    // LDA #$20           (' ')
                  "85 04"    // STA $04
                  "A9 43"    // LDA #$43           ('C')
                  "85 05"    // STA $05
                  "A9 44"    // LDA #$44           ('D')
                  "85 06"    // STA $06
                  "A9 07"    // LDA #$07
                  "85 40"    // STA $40            (LINELEN = 7)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 60"    // LDA #$60
                  "85 F0"    // STA $F0            (ADDR = $0060)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0060), 0xAB);
    EXPECT_EQ(fx.ram.read(0x0061), 0xCD);
}

TEST(MonitorRoutines, DispatchRunsProgramAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // A tiny user program: INC $80 ; RTS
    fx.ram.write(0x0070, 0xE6);
    fx.ram.write(0x0071, 0x80);
    fx.ram.write(0x0072, 0x60);
    // LINEBUF = " R", LINEPOS = 0, LINELEN = 2; $F0/$F1 = $0070.
    fx.loadDriver("A9 20"    // LDA #$20           (' ')
                  "85 00"    // STA $00
                  "A9 52"    // LDA #$52           ('R')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 70"    // LDA #$70
                  "85 F0"    // STA $F0            (ADDR = $0070)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0080), 1);
}

TEST(MonitorRoutines, DispatchPrintsQuestionMarkOnMalformedSuffix) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = "#", LINEPOS = 0, LINELEN = 1 -- '#' is none of '.'/':'/' '.
    fx.loadDriver("A9 23"    // LDA #$23           ('#')
                  "85 00"    // STA $00
                  "A9 01"    // LDA #$01
                  "85 40"    // STA $40            (LINELEN = 1)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}

TEST(MonitorRoutines, RewindResetsPositionAndClearsEot) {
    std::stringstream tape("A");
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
    fx.loadDriver("A9 01"    // LDA #$01
                  "8D 03 80" // STA $8003          (motor on)
                  "AD 04 80" // LDA $8004          (consume the only byte)
                  "AD 04 80" // LDA $8004          (hits EOF -> EOT/ERROR set)
                  "20 00 D2" // JSR $D200          (REWIND)
                  "AD 02 80" // LDA $8002          (STATUS after rewind)
                  "85 50"    // STA $50
                  "A9 01"    // LDA #$01
                  "8D 03 80" // STA $8003          (motor on again)
                  "AD 04 80" // LDA $8004          (re-read -- should be 'A' again)
                  "85 51"    // STA $51
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.ram.read(0x50), 0x01); // PRESENT only -- MOTOR/EOT/ERROR all clear
    EXPECT_EQ(fx.ram.read(0x51), 'A');  // position truly reset to 0
}

TEST(MonitorRoutines, RewindPrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
    fx.loadDriver("20 00 D2" // JSR $D200 (REWIND)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}

TEST(MonitorRoutines, SaveWritesLengthDataAndChecksumToTape) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.ram.write(0x0052, 0xCC);
    fx.loadDriver("A9 50"    // LDA #$50
                  "85 F0"    // STA $F0   (START lo = $50)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1   (START hi)
                  "A9 52"    // LDA #$52
                  "85 F2"    // STA $F2   (END lo = $52)
                  "A9 00"    // LDA #$00
                  "85 F3"    // STA $F3   (END hi)
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(tape.str(), std::string("\x03\x00\xAA\xBB\xCC\xDD", 6));
    EXPECT_EQ(fx.tty.read(0x02) & 0x02, 0); // motor off after SAVE completes
}

TEST(MonitorRoutines, SavePrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 F0"    // STA $F0
                  "85 F1"    // STA $F1
                  "85 F2"    // STA $F2
                  "85 F3"    // STA $F3
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}

TEST(MonitorRoutines, SavePrintsQuestionMarkWhenEndIsBeforeStart) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.loadDriver("A9 52"    // LDA #$52
                  "85 F0"    // STA $F0   (START = $0052)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "A9 50"    // LDA #$50
                  "85 F2"    // STA $F2   (END = $0050, before START)
                  "A9 00"    // LDA #$00
                  "85 F3"    // STA $F3
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(tape.str(), "");
}

TEST(MonitorRoutines, LoadReadsLengthPrefixedBlockIntoRam) {
    std::stringstream tape(std::string("\x03\x00\xAA\xBB\xCC\xDD", 6));
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"    // LDA #$70
                  "85 F0"    // STA $F0   (target lo = $70)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1   (target hi)
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
    EXPECT_EQ(fx.tty.read(0x02) & 0x02, 0); // motor off after LOAD completes
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkAndKeepsPartialDataOnChecksumMismatch) {
    std::stringstream tape(std::string("\x03\x00\xAA\xBB\xCC\x00", 6)); // wrong checksum
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA); // already-read bytes are NOT rolled back
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkWhenTapeRunsOutMidBlock) {
    std::stringstream tape(std::string("\x05\x00\xAA", 3)); // claims 5 bytes, gives 1
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA); // the one byte that was available landed
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}

TEST(MonitorRoutines, DispatchSavesRangeAsBlock) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    // LINEBUF = ".0051 S", LINEPOS = 0, LINELEN = 7; $F0/$F1 already
    // holds the first address ($0050), as if MAIN_LOOP's PARSE_ADDR had
    // already run.
    fx.loadDriver("A9 2E"    // LDA #$2E  ('.')
                  "85 00"    // STA $00
                  "A9 30"    // LDA #$30  ('0')
                  "85 01"    // STA $01
                  "A9 30"    // LDA #$30  ('0')
                  "85 02"    // STA $02
                  "A9 35"    // LDA #$35  ('5')
                  "85 03"    // STA $03
                  "A9 31"    // LDA #$31  ('1')
                  "85 04"    // STA $04
                  "A9 20"    // LDA #$20  (' ')
                  "85 05"    // STA $05
                  "A9 53"    // LDA #$53  ('S')
                  "85 06"    // STA $06
                  "A9 07"    // LDA #$07
                  "85 40"    // STA $40   (LINELEN = 7)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0   (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(tape.str(), std::string("\x02\x00\xAA\xBB\x11", 5)); // LEN=2, data, checksum=0xAA^0xBB=0x11
}

TEST(MonitorRoutines, DispatchLoadsBlockIntoRam) {
    std::stringstream tape(std::string("\x02\x00\xAA\xBB\x11", 5));
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    // LINEBUF = " L", LINEPOS = 0, LINELEN = 2; $F0/$F1 = $0070.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 4C"    // LDA #$4C  ('L')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "A9 70"    // LDA #$70
                  "85 F0"    // STA $F0   (ADDR = $0070)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
}

TEST(MonitorRoutines, DispatchRewindsTape) {
    std::stringstream tape("A");
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    // First, directly exhaust the tape so EOT/ERROR are set (bypassing
    // DISPATCH -- this is just test setup).
    fx.tty.write(0x03, 0x01); // motor on
    fx.tty.read(0x04);        // consume 'A'
    fx.tty.read(0x04);        // hits EOF
    fx.tty.write(0x03, 0x00); // motor off
    // LINEBUF = " W", LINEPOS = 0, LINELEN = 2; $F0/$F1 value is
    // irrelevant to REWIND but required by the grammar.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 57"    // LDA #$57  ('W')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "85 F0"    // STA $F0
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.tty.read(0x02), 0x01); // PRESENT only -- EOT/ERROR cleared, motor off
}

TEST(MonitorRoutines, DispatchStillPrintsQuestionMarkOnUnrecognizedTrailingLetter) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = " X", LINEPOS = 0, LINELEN = 2 -- 'X' is none of R/L/W.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 58"    // LDA #$58  ('X')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}
