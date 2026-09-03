#include "vectordb/bitset.hpp"

#include <gtest/gtest.h>

using vectordb::Bitset;

TEST(BitsetTest, SetTestClearInRange) {
    Bitset bs;
    EXPECT_EQ(bs.size(), 0u);
    EXPECT_FALSE(bs.test(0));

    bs.set(3);
    EXPECT_EQ(bs.size(), 4u);
    EXPECT_TRUE(bs.test(3));
    EXPECT_FALSE(bs.test(0));
    EXPECT_FALSE(bs.test(2));

    bs.clear(3);
    EXPECT_FALSE(bs.test(3));
    EXPECT_EQ(bs.size(), 4u);  // clear does not shrink
}

TEST(BitsetTest, GrowsWhenSettingPastEnd) {
    Bitset bs;
    bs.set(100);
    EXPECT_EQ(bs.size(), 101u);
    EXPECT_TRUE(bs.test(100));
    EXPECT_FALSE(bs.test(99));
    EXPECT_FALSE(bs.test(101));
}

TEST(BitsetTest, BitsInSameWord) {
    Bitset bs;
    bs.set(0);
    bs.set(63);
    EXPECT_TRUE(bs.test(0));
    EXPECT_TRUE(bs.test(63));
    EXPECT_FALSE(bs.test(1));
    EXPECT_FALSE(bs.test(62));
    EXPECT_EQ(bs.size(), 64u);
}

TEST(BitsetTest, BitsSpanTwoWords) {
    Bitset bs;
    bs.set(63);
    bs.set(64);
    EXPECT_TRUE(bs.test(63));
    EXPECT_TRUE(bs.test(64));
    EXPECT_FALSE(bs.test(62));
    EXPECT_FALSE(bs.test(65));
    EXPECT_EQ(bs.size(), 65u);
}

TEST(BitsetTest, ClearAndTestOutOfRangeAreNoOps) {
    Bitset bs;
    bs.set(5);
    bs.clear(10);   // past current size
    bs.clear(5);
    EXPECT_FALSE(bs.test(5));
    EXPECT_FALSE(bs.test(10));
}
