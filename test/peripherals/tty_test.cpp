#include <gtest/gtest.h>

#include "peripherals/tty.h"

#include <sstream>

TEST(TTYTest, StatusRxReadyBitClearUntilAByteArrives) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0) & 0x01, 0);
    tty.receive('A');
    EXPECT_EQ(tty.read(0) & 0x01, 1);
}

TEST(TTYTest, StatusTxReadyBitAlwaysSet) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0) & 0x02, 0x02);
    tty.write(1, 'x');
    EXPECT_EQ(tty.read(0) & 0x02, 0x02);
}

TEST(TTYTest, ReadingDataReturnsReceivedByteAndClearsRxReady) {
    std::ostringstream out;
    TTY tty(out);
    tty.receive(0x42);
    EXPECT_EQ(tty.read(1), 0x42);
    EXPECT_EQ(tty.read(0) & 0x01, 0);
    EXPECT_FALSE(tty.rxReady());
}

TEST(TTYTest, WritingDataReachesOutputStream) {
    std::ostringstream out;
    TTY tty(out);
    tty.write(1, 'H');
    tty.write(1, 'i');
    EXPECT_EQ(out.str(), "Hi");
}

TEST(TTYTest, SecondReceiveBeforeReadIsDropped) {
    std::ostringstream out;
    TTY tty(out);
    tty.receive('A');
    tty.receive('B'); // single holding register -- no FIFO, this is dropped
    EXPECT_EQ(tty.read(1), 'A');
}
