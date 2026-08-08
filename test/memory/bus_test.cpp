#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"

TEST(Bus, AttachAcceptsValidNonOverlappingRanges) {
    Bus bus;
    RAM ram(0x100);
    ROM rom(std::vector<uint8_t>(0x100));

    EXPECT_NO_THROW(bus.attach(0x0000, 0x00FF, ram));
    EXPECT_NO_THROW(bus.attach(0x0100, 0x01FF, rom));
}

TEST(Bus, AttachThrowsOnSizeMismatch) {
    Bus bus;
    RAM ram(0x100);

    EXPECT_THROW(bus.attach(0x0000, 0x00FE, ram), std::invalid_argument);
}

TEST(Bus, AttachThrowsOnOverlappingRange) {
    Bus bus;
    RAM ram1(0x100);
    RAM ram2(0x80);

    bus.attach(0x0000, 0x00FF, ram1);

    EXPECT_THROW(bus.attach(0x0080, 0x00FF, ram2), std::invalid_argument);
}

TEST(Bus, ReadWriteRouteToCorrectDeviceWithTranslatedOffsets) {
    Bus bus;
    RAM ram(0x100);
    ROM rom(std::vector<uint8_t>{0xAA, 0xBB, 0xCC});

    bus.attach(0x0000, 0x00FF, ram);
    bus.attach(0x0100, 0x0102, rom);

    bus.write(0x0010, 0x42);
    EXPECT_EQ(bus.read(0x0010), 0x42);
    EXPECT_EQ(ram.read(0x0010), 0x42);

    EXPECT_EQ(bus.read(0x0100), 0xAA);
    EXPECT_EQ(bus.read(0x0101), 0xBB);
}

TEST(Bus, UnmappedReadReturns0xFF) {
    Bus bus;
    RAM ram(0x10);
    bus.attach(0x0000, 0x000F, ram);

    EXPECT_EQ(bus.read(0x1000), 0xFF);
}

TEST(Bus, UnmappedWriteIsNoOpAndDoesNotCrash) {
    Bus bus;
    RAM ram(0x10);
    bus.attach(0x0000, 0x000F, ram);

    EXPECT_NO_THROW(bus.write(0x1000, 0x99));
}

TEST(Bus, WriteToROMIsIgnoredThroughBus) {
    Bus bus;
    ROM rom(std::vector<uint8_t>{0x01});
    bus.attach(0x0000, 0x0000, rom);

    bus.write(0x0000, 0xFF);

    EXPECT_EQ(bus.read(0x0000), 0x01);
}
