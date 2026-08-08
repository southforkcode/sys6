#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AddressingTestAccess : public CPU6502 {
public:
    using CPU6502::CPU6502;
    using CPU6502::indexedAddress;
};

class CPU6502AddressingTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502AddressingTestAccess cpu{bus};

    CPU6502AddressingTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AddressingTest, IndexedAddressAddsIndexToBaseWithinSamePage) {
    EffectiveAddress result = CPU6502AddressingTestAccess::indexedAddress(0x0200, 0x05);

    EXPECT_EQ(result.address, 0x0205);
    EXPECT_FALSE(result.pageCrossed);
}

TEST_F(CPU6502AddressingTest, IndexedAddressDetectsPageCrossing) {
    EffectiveAddress result = CPU6502AddressingTestAccess::indexedAddress(0x02FF, 0x01);

    EXPECT_EQ(result.address, 0x0300);
    EXPECT_TRUE(result.pageCrossed);
}

TEST_F(CPU6502AddressingTest, IndexedAddressWrapsAt16BitBoundary) {
    EffectiveAddress result = CPU6502AddressingTestAccess::indexedAddress(0xFFFF, 0x01);

    EXPECT_EQ(result.address, 0x0000);
}

TEST_F(CPU6502AddressingTest, IndexedAddressWithZeroIndexNeverCrosses) {
    EffectiveAddress result = CPU6502AddressingTestAccess::indexedAddress(0x0200, 0x00);

    EXPECT_EQ(result.address, 0x0200);
    EXPECT_FALSE(result.pageCrossed);
}
