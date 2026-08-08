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
