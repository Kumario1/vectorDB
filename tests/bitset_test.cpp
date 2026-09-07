#include "vectordb/bitset.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

using vectordb::Bitset;

namespace {

// Helpers for building the learning-note example quickly.
Bitset from_slots(std::initializer_list<std::size_t> slots) {
    Bitset bs;
    for (std::size_t s : slots) {
        bs.set(s);
    }
    return bs;
}

}  // namespace

// ---------------------------------------------------------------------------
// M9.2 — already green (leave these alone)
// ---------------------------------------------------------------------------

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
    EXPECT_EQ(bs.size(), 4u);
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
    bs.clear(10);
    bs.clear(5);
    EXPECT_FALSE(bs.test(5));
    EXPECT_FALSE(bs.test(10));
}

// ---------------------------------------------------------------------------
// M9.3 — these should FAIL on stubs; turn them green one by one
// ---------------------------------------------------------------------------

// Learning note hand example:
//   book: 1 0 1 1 0 0 1 0  → slots 0, 2, 3, 6
//   en:   1 1 1 0 0 1 1 0  → slots 0, 1, 2, 5, 6
//   AND:  1 0 1 0 0 0 1 0  → slots 0, 2, 6
TEST(BitsetTest, AndBookTimesEn) {
    Bitset book = from_slots({0, 2, 3, 6});
    Bitset en = from_slots({0, 1, 2, 5, 6});

    Bitset both = book & en;
    EXPECT_EQ(both.set_bits(), (std::vector<std::size_t>{0, 2, 6}));
    EXPECT_EQ(both.count(), 3u);
}

TEST(BitsetTest, AndDisjointIsEmpty) {
    Bitset a = from_slots({0, 2});
    Bitset b = from_slots({1, 3});
    Bitset out = a & b;
    EXPECT_TRUE(out.set_bits().empty());
    EXPECT_EQ(out.count(), 0u);
}

TEST(BitsetTest, AndWithEmpty) {
    Bitset a = from_slots({0, 1, 2});
    Bitset empty;
    EXPECT_TRUE((a & empty).set_bits().empty());
    EXPECT_TRUE((empty & a).set_bits().empty());
}

TEST(BitsetTest, OrUnion) {
    Bitset book = from_slots({0, 2, 3, 6});
    Bitset en = from_slots({0, 1, 2, 5, 6});
    Bitset either = book | en;
    EXPECT_EQ(either.set_bits(), (std::vector<std::size_t>{0, 1, 2, 3, 5, 6}));
    EXPECT_EQ(either.count(), 6u);
}

TEST(BitsetTest, XorSymmetricDifference) {
    Bitset book = from_slots({0, 2, 3, 6});
    Bitset en = from_slots({0, 1, 2, 5, 6});
    // in book xor en: 1, 3, 5  (in one but not both)
    Bitset x = book ^ en;
    EXPECT_EQ(x.set_bits(), (std::vector<std::size_t>{1, 3, 5}));
}

TEST(BitsetTest, NotFlipsOnlyLiveBits) {
    // size becomes 4 after set(3); bits 0..3 exist: only bit 1 set → 0100
    Bitset bs = from_slots({1});
    // force size 4: set then clear a high bit, or set(3) then clear(3)
    bs.set(3);
    bs.clear(3);
    // now size==4, only bit 1 is on → ~ should turn on 0, 2, 3 (not junk past 3)
    Bitset flipped = ~bs;
    EXPECT_EQ(flipped.size(), 4u);
    EXPECT_EQ(flipped.set_bits(), (std::vector<std::size_t>{0, 2, 3}));
    EXPECT_EQ(flipped.count(), 3u);
}

TEST(BitsetTest, CountMatchesSetBitsSize) {
    Bitset bs = from_slots({0, 2, 3, 6, 64});  // spans two words
    EXPECT_EQ(bs.count(), bs.set_bits().size());
    EXPECT_EQ(bs.set_bits(), (std::vector<std::size_t>{0, 2, 3, 6, 64}));
}

TEST(BitsetTest, AndAcrossTwoWords) {
    Bitset a = from_slots({63, 64});
    Bitset b = from_slots({64, 65});
    Bitset out = a & b;
    EXPECT_EQ(out.set_bits(), (std::vector<std::size_t>{64}));
}
