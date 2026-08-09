#include <gtest/gtest.h>

#include "peripherals/tty.h"

#include <sstream>

namespace {
constexpr uint16_t kTapeStatus = 0x02;
constexpr uint16_t kTapeControl = 0x03;
constexpr uint16_t kTapeData = 0x04;
constexpr uint8_t kPresent = 0x01;
constexpr uint8_t kMotor = 0x02;
constexpr uint8_t kEot = 0x04;
constexpr uint8_t kError = 0x08;
} // namespace

TEST(TTYTapeTest, SizeIsA256BytePage) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.size(), 256u);
}

TEST(TTYTapeTest, ReservedOffsetsReadZeroAndIgnoreWrites) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0x05), 0);
    EXPECT_EQ(tty.read(0xFF), 0);
    tty.write(0x05, 0xAB);
    EXPECT_EQ(tty.read(0x05), 0);
}

TEST(TTYTapeTest, PresentBitReflectsWhetherABackingWasGiven) {
    std::ostringstream out;
    TTY noTape(out);
    EXPECT_EQ(noTape.read(kTapeStatus) & kPresent, 0);

    std::stringstream tape;
    TTY withTape(out, &tape);
    EXPECT_EQ(withTape.read(kTapeStatus) & kPresent, kPresent);
}

TEST(TTYTapeTest, ControlWriteSetsAndClearsMotorBitInStatus) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, 0);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, kMotor);
    tty.write(kTapeControl, 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, 0);
}

TEST(TTYTapeTest, DataReadWriteRoundTripsThroughTheBackingWithMotorOn) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01); // motor on
    tty.write(kTapeData, 0xAA);
    tty.write(kTapeData, 0xBB);
    tty.write(kTapeControl, 0x02); // rewind (also turns motor off)
    tty.write(kTapeControl, 0x01); // motor back on
    EXPECT_EQ(tty.read(kTapeData), 0xAA);
    EXPECT_EQ(tty.read(kTapeData), 0xBB);
}

TEST(TTYTapeTest, ReadPastEndSetsEotAndErrorWithoutAdvancing) {
    std::ostringstream out;
    std::stringstream tape("A");
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 'A');
    EXPECT_EQ(tty.read(kTapeData), 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kEot, kEot);
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
}

TEST(TTYTapeTest, DataAccessWithMotorOffSetsErrorAndDoesNothing) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeData, 0xAA); // motor never turned on
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 0x00); // nothing was ever written
}

TEST(TTYTapeTest, DataAccessWithNoBackingSetsError) {
    std::ostringstream out;
    TTY tty(out); // no tape backing at all
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
}

TEST(TTYTapeTest, WritePastCurrentEndExtendsTheBacking) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    tty.write(kTapeData, 0x11);
    tty.write(kTapeData, 0x22);
    EXPECT_EQ(tape.str(), std::string("\x11\x22", 2));
}

TEST(TTYTapeTest, SuccessfulReadClearsAPreviousEot) {
    std::ostringstream out;
    std::stringstream tape("A");
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    tty.read(kTapeData);         // consume the only byte
    tty.read(kTapeData);         // EOT now set
    ASSERT_EQ(tty.read(kTapeStatus) & kEot, kEot);
    tty.write(kTapeControl, 0x02); // rewind clears EOT
    tty.write(kTapeControl, 0x01);
    tty.read(kTapeData);         // succeeds again
    EXPECT_EQ(tty.read(kTapeStatus) & kEot, 0);
}
