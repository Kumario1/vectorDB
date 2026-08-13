#include "vectordb/memtable.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using vectordb::Memtable;
using vectordb::Status;

TEST(MemtableTest, StartsEmpty) {
    Memtable mt(2, 1024);
    EXPECT_EQ(mt.size(), 0u);
    EXPECT_EQ(mt.live_count(), 0u);
    EXPECT_FALSE(mt.get(1).has_value());
    EXPECT_FALSE(mt.needs_flush());
}

TEST(MemtableTest, PutAndGetRoundTrip) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(42, {1.0f, 0.0f}), Status::ok);

    const auto got = mt.get(42);
    ASSERT_TRUE(got.has_value());
    ASSERT_EQ(got->size(), 2u);
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
    EXPECT_FLOAT_EQ((*got)[1], 0.0f);
    EXPECT_EQ(mt.size(), 1u);
    EXPECT_EQ(mt.live_count(), 1u);
}

TEST(MemtableTest, PutOverwritesSameId) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(7, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(mt.put(7, {0.0f, 1.0f}), Status::ok);

    EXPECT_EQ(mt.size(), 1u);
    const auto got = mt.get(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);
}

TEST(MemtableTest, WrongDimensionsReturnsMismatch) {
    Memtable mt(2);
    EXPECT_EQ(mt.put(1, {1.0f}), Status::dimension_mismatch);
    EXPECT_EQ(mt.put(1, {1.0f, 2.0f, 3.0f}), Status::dimension_mismatch);
    EXPECT_EQ(mt.size(), 0u);
}

TEST(MemtableTest, GetMissingReturnsEmpty) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(1, {1.0f, 0.0f}), Status::ok);
    EXPECT_FALSE(mt.get(2).has_value());
}

TEST(MemtableTest, RemoveMakesGetEmptyButKeepsSize) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(42, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(mt.remove(42), Status::ok);

    EXPECT_FALSE(mt.get(42).has_value());
    EXPECT_EQ(mt.size(), 1u);       // tombstone still occupies a row
    EXPECT_EQ(mt.live_count(), 0u);
}

TEST(MemtableTest, RemoveMissingReturnsNotFound) {
    Memtable mt(2);
    EXPECT_EQ(mt.remove(99), Status::not_found);
}

TEST(MemtableTest, RemoveTwiceIsOk) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(5, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(mt.remove(5), Status::ok);
    EXPECT_EQ(mt.remove(5), Status::ok);
    EXPECT_EQ(mt.size(), 1u);
    EXPECT_EQ(mt.live_count(), 0u);
}

TEST(MemtableTest, PutAfterTombstoneResurrects) {
    Memtable mt(2);
    ASSERT_EQ(mt.put(5, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(mt.remove(5), Status::ok);
    ASSERT_EQ(mt.put(5, {0.0f, 1.0f}), Status::ok);

    EXPECT_EQ(mt.size(), 1u);
    EXPECT_EQ(mt.live_count(), 1u);
    const auto got = mt.get(5);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);
}

TEST(MemtableTest, NeedsFlushUsesRowCountIncludingTombstones) {
    Memtable mt(2, /*flush_threshold_rows=*/2);
    EXPECT_FALSE(mt.needs_flush());

    ASSERT_EQ(mt.put(1, {1.0f, 0.0f}), Status::ok);
    EXPECT_FALSE(mt.needs_flush());

    ASSERT_EQ(mt.put(2, {0.0f, 1.0f}), Status::ok);
    EXPECT_TRUE(mt.needs_flush());

    ASSERT_EQ(mt.remove(1), Status::ok);
    EXPECT_TRUE(mt.needs_flush());  // tombstone still counts
    EXPECT_EQ(mt.size(), 2u);
    EXPECT_EQ(mt.live_count(), 1u);
}

TEST(MemtableTest, IterationIsSortedById) {
    Memtable mt(1);
    ASSERT_EQ(mt.put(30, {3.0f}), Status::ok);
    ASSERT_EQ(mt.put(10, {1.0f}), Status::ok);
    ASSERT_EQ(mt.put(20, {2.0f}), Status::ok);

    std::vector<std::uint64_t> ids;
    for (const auto& [id, entry] : mt) {
        ids.push_back(id);
    }
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 10u);
    EXPECT_EQ(ids[1], 20u);
    EXPECT_EQ(ids[2], 30u);
}

TEST(MemtableTest, IterationIncludesTombstones) {
    Memtable mt(1);
    ASSERT_EQ(mt.put(2, {2.0f}), Status::ok);
    ASSERT_EQ(mt.put(1, {1.0f}), Status::ok);
    ASSERT_EQ(mt.remove(1), Status::ok);

    auto it = mt.begin();
    ASSERT_NE(it, mt.end());
    EXPECT_EQ(it->first, 1u);
    EXPECT_TRUE(it->second.is_deleted);
    EXPECT_TRUE(it->second.values.empty());

    ++it;
    ASSERT_NE(it, mt.end());
    EXPECT_EQ(it->first, 2u);
    EXPECT_FALSE(it->second.is_deleted);
    ++it;
    EXPECT_EQ(it, mt.end());
}
