#include <gtest/gtest.h>

#include "memory/rom.h"

TEST(ROM, SizeReflectsConstructorData) {
    ROM rom(std::vector<uint8_t>{1, 2, 3, 4});
    EXPECT_EQ(rom.size(), 4u);
}

TEST(ROM, ReadReturnsConstructorContents) {
    ROM rom(std::vector<uint8_t>{0x10, 0x20, 0x30});

    EXPECT_EQ(rom.read(0), 0x10);
    EXPECT_EQ(rom.read(1), 0x20);
    EXPECT_EQ(rom.read(2), 0x30);
}

TEST(ROM, WriteIsIgnored) {
    ROM rom(std::vector<uint8_t>{0xAA, 0xBB});

    rom.write(0, 0xFF);

    EXPECT_EQ(rom.read(0), 0xAA);
}

TEST(ROM, WriteWithoutLoggerDoesNotCrash) {
    ROM rom(std::vector<uint8_t>{0xAA});
    rom.write(0, 0xFF);
    EXPECT_EQ(rom.read(0), 0xAA);
}

TEST(ROM, LoadBypassesTheWriteIgnoreBehavior) {
    ROM rom(std::vector<uint8_t>{0xAA, 0xBB});

    rom.load(0, 0xFF);

    EXPECT_EQ(rom.read(0), 0xFF);
    EXPECT_EQ(rom.read(1), 0xBB);
}
