#include "vectordb/wal.hpp"

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

class WalTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_wal_test";
        fs::create_directories(dir_);
        path_ = (dir_ / "test.wal").string();
        fs::remove(path_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    fs::path dir_;
    std::string path_;
};

TEST_F(WalTest, AppendTwoInsertsRoundTrip) {
    {
        vectordb::WalWriter writer(path_);
        ASSERT_EQ(writer.open(), vectordb::Status::ok);

        vectordb::WalRecord a;
        a.op = vectordb::WalOp::Insert;
        a.id = 42;
        a.dimensions = 2;
        a.values = {1.0f, 2.0f};
        ASSERT_EQ(writer.append(a), vectordb::Status::ok);
        EXPECT_EQ(a.lsn, 1u);

        vectordb::WalRecord b;
        b.op = vectordb::WalOp::Insert;
        b.id = 99;
        b.dimensions = 2;
        b.values = {3.0f, 4.0f};
        ASSERT_EQ(writer.append(b), vectordb::Status::ok);
        EXPECT_EQ(b.lsn, 2u);

        ASSERT_EQ(writer.flush(), vectordb::Status::ok);
    }

    vectordb::WalReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);

    vectordb::WalRecord got;
    bool have = false;

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    ASSERT_TRUE(have);
    EXPECT_EQ(got.lsn, 1u);
    EXPECT_EQ(got.op, vectordb::WalOp::Insert);
    EXPECT_EQ(got.id, 42u);
    EXPECT_EQ(got.dimensions, 2u);
    ASSERT_EQ(got.values.size(), 2u);
    EXPECT_EQ(got.values[0], 1.0f);
    EXPECT_EQ(got.values[1], 2.0f);

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    ASSERT_TRUE(have);
    EXPECT_EQ(got.lsn, 2u);
    EXPECT_EQ(got.id, 99u);
    ASSERT_EQ(got.values.size(), 2u);
    EXPECT_EQ(got.values[0], 3.0f);
    EXPECT_EQ(got.values[1], 4.0f);

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    EXPECT_FALSE(have);
}

TEST_F(WalTest, RejectsDimensionMismatch) {
    vectordb::WalWriter writer(path_);
    ASSERT_EQ(writer.open(), vectordb::Status::ok);

    vectordb::WalRecord bad;
    bad.op = vectordb::WalOp::Insert;
    bad.id = 1;
    bad.dimensions = 2;
    bad.values = {1.0f};  // wrong size
    EXPECT_EQ(writer.append(bad), vectordb::Status::dimension_mismatch);
}

TEST_F(WalTest, RejectsUnsupportedOp) {
    vectordb::WalWriter writer(path_);
    ASSERT_EQ(writer.open(), vectordb::Status::ok);

    vectordb::WalRecord del;
    del.op = vectordb::WalOp::Delete;
    del.id = 1;
    EXPECT_EQ(writer.append(del), vectordb::Status::invalid_argument);
}

TEST_F(WalTest, MissingFileOpenFails) {
    const auto missing = (dir_ / "nope.wal").string();
    vectordb::WalReader reader(missing);
    EXPECT_EQ(reader.open(), vectordb::Status::invalid_argument);
}

TEST_F(WalTest, EmptyFileEof) {
    {
        // create empty file via writer open + flush with no appends
        vectordb::WalWriter writer(path_);
        ASSERT_EQ(writer.open(), vectordb::Status::ok);
        ASSERT_EQ(writer.flush(), vectordb::Status::ok);
    }
    vectordb::WalReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    vectordb::WalRecord got;
    bool have = true;
    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    EXPECT_FALSE(have);
}
