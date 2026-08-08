#include <gtest/gtest.h>

#include "memory/ram.h"
#include "utils/program_loader.h"

#include <stdexcept>

TEST(ProgramLoader, LoadsWhitespaceSeparatedHexIntoRam) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "A9 05 8D 00 02");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0x05);
    EXPECT_EQ(ram.read(2), 0x8D);
    EXPECT_EQ(ram.read(3), 0x00);
    EXPECT_EQ(ram.read(4), 0x02);
}

TEST(ProgramLoader, LoadsUnseparatedHexIntoRam) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "A9058D0002");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0x05);
    EXPECT_EQ(ram.read(2), 0x8D);
    EXPECT_EQ(ram.read(3), 0x00);
    EXPECT_EQ(ram.read(4), 0x02);
}

TEST(ProgramLoader, LoadsLowercaseHex) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "a9 ff");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0xFF);
}

TEST(ProgramLoader, WritesStartingAtGivenAddress) {
    RAM ram(16);

    loadProgram(ram, 0x0005, "AA BB");

    EXPECT_EQ(ram.read(5), 0xAA);
    EXPECT_EQ(ram.read(6), 0xBB);
}

TEST(ProgramLoader, ThrowsOnOddLengthHex) {
    RAM ram(16);

    EXPECT_THROW(loadProgram(ram, 0x0000, "A9B"), std::invalid_argument);
}

TEST(ProgramLoader, ThrowsOnNonHexCharacter) {
    RAM ram(16);

    EXPECT_THROW(loadProgram(ram, 0x0000, "ZZ"), std::invalid_argument);
}

TEST(ProgramLoader, EmptyStringWritesNothing) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "");

    EXPECT_EQ(ram.read(0), 0x00);
}
