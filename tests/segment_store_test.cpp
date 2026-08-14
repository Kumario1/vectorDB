#include "vectordb/segment_store.hpp"
#include "vectordb/manifest.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

using vectordb::Metric;
using vectordb::SegmentStore;
using vectordb::Status;

class SegmentStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_segment_store_test";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    fs::path dir_;
};

TEST_F(SegmentStoreTest, GetFromMemtableOnly) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);

    const auto got = store.get(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
    EXPECT_FLOAT_EQ((*got)[1], 0.0f);
}

TEST_F(SegmentStoreTest, GetMissingReturnsEmpty) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    EXPECT_FALSE(store.get(1).has_value());
}

TEST_F(SegmentStoreTest, PutDimensionMismatch) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    EXPECT_EQ(store.put(1, {1.0f}), Status::dimension_mismatch);
}

TEST_F(SegmentStoreTest, GetFromSegmentAfterFlush) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(42, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    EXPECT_FALSE(store.get(99).has_value());
    const auto got = store.get(42);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);
}

TEST_F(SegmentStoreTest, MemtableOverridesOlderSegment) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(7, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(7, {0.0f, 1.0f}), Status::ok);

    const auto got = store.get(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);
}

TEST_F(SegmentStoreTest, NewerSegmentOverridesOlderSegment) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(7, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(7, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    const auto got = store.get(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);
}

TEST_F(SegmentStoreTest, FlushThenNewWriteAllVisible) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(2, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(3, {1.0f, 1.0f}), Status::ok);

    EXPECT_TRUE(store.get(1).has_value());
    EXPECT_TRUE(store.get(2).has_value());
    EXPECT_TRUE(store.get(3).has_value());
}

TEST_F(SegmentStoreTest, MemtableTombstoneHidesOlderSegment) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(5, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.remove(5), Status::ok);

    EXPECT_FALSE(store.get(5).has_value());
}

TEST_F(SegmentStoreTest, RemoveAbsentIdStillOk) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    EXPECT_EQ(store.remove(123), Status::ok);
    EXPECT_FALSE(store.get(123).has_value());
}

TEST_F(SegmentStoreTest, FlushedTombstoneHidesOlderSegment) {
    // #14 path: tombstone lives in a newer segment, not memtable
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(5, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.remove(5), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    EXPECT_FALSE(store.get(5).has_value());

    const auto results = store.search({1.0f, 0.0f}, 10);
    for (const auto& r : results) {
        EXPECT_NE(r.id, 5u);
    }
}

TEST_F(SegmentStoreTest, TombstoneHidesMultipleOlderVersions) {
    // live in seg1, updated in seg2, deleted in seg3 → gone
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);

    ASSERT_EQ(store.put(8, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    ASSERT_EQ(store.put(8, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    ASSERT_EQ(store.remove(8), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    EXPECT_FALSE(store.get(8).has_value());

    // sibling id still visible from an older segment
    ASSERT_EQ(store.put(9, {3.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    // reopen path: only segments
    SegmentStore reopened(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(reopened.open(), Status::ok);
    EXPECT_FALSE(reopened.get(8).has_value());
    const auto got9 = reopened.get(9);
    ASSERT_TRUE(got9.has_value());
    EXPECT_FLOAT_EQ((*got9)[0], 3.0f);

    const auto results = reopened.search({1.0f, 0.0f}, 10);
    for (const auto& r : results) {
        EXPECT_NE(r.id, 8u);
    }
}

TEST_F(SegmentStoreTest, SearchMatchesGetVisibility) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(2, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(3, {2.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.remove(2), Status::ok);

    const auto results = store.search({1.0f, 0.0f}, 10);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].id, 3u);
    EXPECT_EQ(results[1].id, 1u);
    for (const auto& r : results) {
        EXPECT_NE(r.id, 2u);
    }
}

TEST_F(SegmentStoreTest, SearchTopKHeap) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(2, {2.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(3, {3.0f, 0.0f}), Status::ok);

    const auto results = store.search({1.0f, 0.0f}, 2);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].id, 3u);
    EXPECT_EQ(results[1].id, 2u);
}

TEST_F(SegmentStoreTest, SearchKZeroOrBadDimsReturnsEmpty) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);

    EXPECT_TRUE(store.search({1.0f, 0.0f}, 0).empty());
    EXPECT_TRUE(store.search({1.0f}, 1).empty());
}

TEST_F(SegmentStoreTest, SearchFewerThanKReturnsAll) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);

    const auto results = store.search({1.0f, 0.0f}, 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, 1u);
}

TEST_F(SegmentStoreTest, SearchCosineAndEuclidean) {
    {
        SegmentStore store(dir_.string(), 2, Metric::cosine);
        ASSERT_EQ(store.open(), Status::ok);
        ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
        ASSERT_EQ(store.put(2, {0.0f, 1.0f}), Status::ok);
        const auto results = store.search({1.0f, 0.0f}, 2);
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0].id, 1u);
    }
    {
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        SegmentStore store(dir_.string(), 2, Metric::euclidean);
        ASSERT_EQ(store.open(), Status::ok);
        ASSERT_EQ(store.put(1, {0.0f, 0.0f}), Status::ok);
        ASSERT_EQ(store.put(2, {10.0f, 10.0f}), Status::ok);
        const auto results = store.search({0.0f, 0.0f}, 1);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].id, 1u);
    }
}

TEST_F(SegmentStoreTest, SearchOverSegmentsOnly) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(2, {2.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);

    const auto results = store.search({1.0f, 0.0f}, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, 2u);
}

TEST_F(SegmentStoreTest, EmptyFlushOk) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    EXPECT_FALSE(store.get(1).has_value());
}

TEST_F(SegmentStoreTest, ReopenLoadsManifest) {
    {
        SegmentStore store(dir_.string(), 2);
        ASSERT_EQ(store.open(), Status::ok);
        ASSERT_EQ(store.put(9, {0.5f, 0.5f}), Status::ok);
        ASSERT_EQ(store.flush(), Status::ok);
    }

    SegmentStore reopened(dir_.string(), 2);
    ASSERT_EQ(reopened.open(), Status::ok);
    const auto got = reopened.get(9);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.5f);
}

TEST_F(SegmentStoreTest, ReopenSeesFlushedTombstone) {
    {
        SegmentStore store(dir_.string(), 2);
        ASSERT_EQ(store.open(), Status::ok);
        ASSERT_EQ(store.put(9, {1.0f, 0.0f}), Status::ok);
        ASSERT_EQ(store.flush(), Status::ok);
        ASSERT_EQ(store.remove(9), Status::ok);
        ASSERT_EQ(store.flush(), Status::ok);
    }

    SegmentStore reopened(dir_.string(), 2);
    ASSERT_EQ(reopened.open(), Status::ok);
    EXPECT_FALSE(reopened.get(9).has_value());
}

TEST_F(SegmentStoreTest, CompactNoOpWithFewerThanTwoSegments) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    EXPECT_EQ(store.compact(), Status::ok);

    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    EXPECT_EQ(store.compact(), Status::ok);

    const auto got = store.get(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
}

TEST_F(SegmentStoreTest, CompactMergesSegmentsAndDropsTombstones) {
    SegmentStore store(dir_.string(), 2, Metric::dot_product);
    ASSERT_EQ(store.open(), Status::ok);

    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.put(2, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);  // segment-000001

    ASSERT_EQ(store.put(1, {2.0f, 0.0f}), Status::ok);  // newer value for 1
    ASSERT_EQ(store.flush(), Status::ok);  // segment-000002

    ASSERT_EQ(store.remove(2), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);  // segment-000003 tombstone for 2

    ASSERT_EQ(store.compact(), Status::ok);

    vectordb::Manifest man;
    ASSERT_EQ(man.load(dir_.string()), Status::ok);
    ASSERT_EQ(man.segments().size(), 1u);
    EXPECT_EQ(man.segments()[0], "segment-000004.vec");

    EXPECT_FALSE(fs::exists(dir_ / "segment-000001.vec"));
    EXPECT_FALSE(fs::exists(dir_ / "segment-000002.vec"));
    EXPECT_FALSE(fs::exists(dir_ / "segment-000003.vec"));
    EXPECT_TRUE(fs::exists(dir_ / "segment-000004.vec"));

    const auto got1 = store.get(1);
    ASSERT_TRUE(got1.has_value());
    EXPECT_FLOAT_EQ((*got1)[0], 2.0f);
    EXPECT_FALSE(store.get(2).has_value());
}

TEST_F(SegmentStoreTest, CompactPreservesMemtable) {
    SegmentStore store(dir_.string(), 2);
    ASSERT_EQ(store.open(), Status::ok);
    ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(2, {0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(store.flush(), Status::ok);
    ASSERT_EQ(store.put(3, {1.0f, 1.0f}), Status::ok);  // only in memtable

    ASSERT_EQ(store.compact(), Status::ok);

    EXPECT_TRUE(store.get(1).has_value());
    EXPECT_TRUE(store.get(2).has_value());
    const auto got3 = store.get(3);
    ASSERT_TRUE(got3.has_value());
    EXPECT_FLOAT_EQ((*got3)[0], 1.0f);
    EXPECT_FLOAT_EQ((*got3)[1], 1.0f);
}

TEST_F(SegmentStoreTest, ReopenAfterCompact) {
    {
        SegmentStore store(dir_.string(), 2);
        ASSERT_EQ(store.open(), Status::ok);
        ASSERT_EQ(store.put(1, {1.0f, 0.0f}), Status::ok);
        ASSERT_EQ(store.flush(), Status::ok);
        ASSERT_EQ(store.put(1, {0.0f, 1.0f}), Status::ok);
        ASSERT_EQ(store.flush(), Status::ok);
        ASSERT_EQ(store.compact(), Status::ok);
    }

    SegmentStore reopened(dir_.string(), 2);
    ASSERT_EQ(reopened.open(), Status::ok);
    const auto got = reopened.get(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 0.0f);
    EXPECT_FLOAT_EQ((*got)[1], 1.0f);

    vectordb::Manifest man;
    ASSERT_EQ(man.load(dir_.string()), Status::ok);
    EXPECT_EQ(man.segments().size(), 1u);
}
