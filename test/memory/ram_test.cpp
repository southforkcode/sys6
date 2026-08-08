#include <gtest/gtest.h>

#include "memory/ram.h"

TEST(RAM, IsZeroInitializedAndReportsConstructorSize) {
    RAM ram(16);

    EXPECT_EQ(ram.size(), 16u);
    for (uint16_t offset = 0; offset < 16; ++offset) {
        EXPECT_EQ(ram.read(offset), 0);
    }
}

TEST(RAM, WriteThenReadRoundTrips) {
    RAM ram(16);

    ram.write(5, 0xAB);
    EXPECT_EQ(ram.read(5), 0xAB);
}

TEST(RAM, BoundaryOffsetsRoundTrip) {
    RAM ram(16);

    ram.write(0, 0x11);
    ram.write(15, 0x22);

    EXPECT_EQ(ram.read(0), 0x11);
    EXPECT_EQ(ram.read(15), 0x22);
}

TEST(RAM, SupportsFullSixtyFourKilobyteSize) {
    RAM ram(0x10000);

    EXPECT_EQ(ram.size(), 0x10000u);
    ram.write(0xFFFF, 0x42);
    EXPECT_EQ(ram.read(0xFFFF), 0x42);
}
