#include "vectordb/database.hpp"

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

using vectordb::Metric;
using vectordb::Status;
using vectordb::StorageMode;
using vectordb::VectorDB;

class VectorDbLsmTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_lsm_test";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    fs::path dir_;
};

TEST_F(VectorDbLsmTest, DefaultCtorIsLsm) {
    VectorDB db(2);
    EXPECT_EQ(db.storage_mode(), StorageMode::lsm);
    EXPECT_EQ(db.size(), 0u);

    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    ASSERT_EQ(db.insert(1, a), Status::ok);
    ASSERT_EQ(db.insert(2, b), Status::ok);
    EXPECT_EQ(db.size(), 2u);

    const auto got = db.get(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
    EXPECT_FLOAT_EQ((*got)[1], 0.0f);

    const auto hits = db.search(a, 1);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].id, 1u);

    EXPECT_EQ(db.insert(1, b), Status::duplicate_id);
    EXPECT_EQ(db.update(99, a), Status::not_found);
    EXPECT_EQ(db.remove(99), Status::not_found);

    ASSERT_EQ(db.remove(1), Status::ok);
    EXPECT_FALSE(db.get(1).has_value());
    EXPECT_EQ(db.size(), 1u);
    ASSERT_EQ(db.insert(1, b), Status::ok);
    EXPECT_EQ(db.size(), 2u);
    const auto got_again = db.get(1);
    ASSERT_TRUE(got_again.has_value());
    EXPECT_FLOAT_EQ((*got_again)[0], 0.0f);
}

TEST_F(VectorDbLsmTest, OpenFlushReopenRecoversFlushedRowsOnly) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    {
        VectorDB db(2);
        ASSERT_EQ(db.open_lsm(dir_.string()), Status::ok);
        ASSERT_EQ(db.insert(1, a), Status::ok);
        ASSERT_EQ(db.flush(), Status::ok);
        ASSERT_EQ(db.insert(2, b), Status::ok);  // unflushed
    }

    VectorDB reopened(2);
    ASSERT_EQ(reopened.open_lsm(dir_.string()), Status::ok);
    EXPECT_TRUE(reopened.get(1).has_value());
    EXPECT_FALSE(reopened.get(2).has_value());  // unflushed rows are gone
    EXPECT_EQ(reopened.size(), 1u);
}

TEST_F(VectorDbLsmTest, InsertPastThresholdAutoFlushesWhenDirOpen) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    {
        VectorDB db(2, Metric::cosine, StorageMode::lsm, /*flush_threshold_rows=*/2);
        ASSERT_EQ(db.open_lsm(dir_.string()), Status::ok);
        ASSERT_EQ(db.insert(1, a), Status::ok);
        ASSERT_EQ(db.insert(2, b), Status::ok);
    }

    VectorDB reopened(2);
    ASSERT_EQ(reopened.open_lsm(dir_.string()), Status::ok);
    EXPECT_TRUE(reopened.get(1).has_value());
    EXPECT_TRUE(reopened.get(2).has_value());
    EXPECT_EQ(reopened.size(), 2u);
}

TEST_F(VectorDbLsmTest, EnableWalAndSaveInvalidOnLsm) {
    VectorDB db(2);
    EXPECT_EQ(db.enable_wal((dir_ / "x.wal").string()), Status::invalid_argument);
    EXPECT_EQ(db.save((dir_ / "x.vdb").string()), Status::invalid_argument);
    EXPECT_EQ(db.load((dir_ / "x.vdb").string()), Status::invalid_argument);
}

TEST_F(VectorDbLsmTest, CompactAfterOpenStillReturnsCorrectGets) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    const float a2[] = {2.0f, 0.0f};
    VectorDB db(2);
    ASSERT_EQ(db.open_lsm(dir_.string()), Status::ok);
    ASSERT_EQ(db.insert(1, a), Status::ok);
    ASSERT_EQ(db.insert(2, b), Status::ok);
    ASSERT_EQ(db.flush(), Status::ok);
    ASSERT_EQ(db.update(1, a2), Status::ok);
    ASSERT_EQ(db.flush(), Status::ok);
    ASSERT_EQ(db.compact(), Status::ok);

    const auto got1 = db.get(1);
    ASSERT_TRUE(got1.has_value());
    EXPECT_FLOAT_EQ((*got1)[0], 2.0f);
    EXPECT_TRUE(db.get(2).has_value());
}

TEST_F(VectorDbLsmTest, LegacySaveLoadSmoke) {
    const auto path = (dir_ / "smoke.vdb").string();
    const float v[] = {3.0f, 4.0f};
    {
        VectorDB db(2, Metric::cosine, StorageMode::legacy);
        ASSERT_EQ(db.insert(7, v), Status::ok);
        ASSERT_EQ(db.save(path), Status::ok);
    }

    VectorDB loaded(2, Metric::cosine, StorageMode::legacy);
    ASSERT_EQ(loaded.load(path), Status::ok);
    EXPECT_EQ(loaded.size(), 1u);
    const auto got = loaded.get(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 3.0f);
    EXPECT_FLOAT_EQ((*got)[1], 4.0f);
}

TEST_F(VectorDbLsmTest, LegacyRejectsLsmApis) {
    VectorDB db(2, Metric::cosine, StorageMode::legacy);
    EXPECT_EQ(db.open_lsm(dir_.string()), Status::invalid_argument);
    EXPECT_EQ(db.flush(), Status::invalid_argument);
    EXPECT_EQ(db.compact(), Status::invalid_argument);
}

TEST_F(VectorDbLsmTest, FlushIsNoOpUntilDirOpened) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    ASSERT_EQ(db.insert(1, v), Status::ok);
    EXPECT_EQ(db.flush(), Status::ok);
    EXPECT_EQ(db.compact(), Status::invalid_argument);
}
