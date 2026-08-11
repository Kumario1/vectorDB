#include "vectordb/segment.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

class SegmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_segment_test";
        fs::create_directories(dir_);
        path_ = (dir_ / "test.vec").string();
        fs::remove(path_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    fs::path dir_;
    std::string path_;
};

TEST_F(SegmentTest, EmptySegmentRoundTrip) {
    {
        vectordb::SegmentWriter writer(path_);
        ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);
        ASSERT_EQ(writer.finish(), vectordb::Status::ok);
    }

    vectordb::SegmentReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    std::vector<vectordb::SegmentRow> rows;
    ASSERT_EQ(reader.read_all(rows), vectordb::Status::ok);
    EXPECT_TRUE(rows.empty());
}

TEST_F(SegmentTest, TwoLiveVectorsRoundTrip) {
    {
        vectordb::SegmentWriter writer(path_);
        ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);

        vectordb::SegmentRow a;
        a.id = 101;
        a.values = {0.1f, 0.2f};
        ASSERT_EQ(writer.append(a), vectordb::Status::ok);

        vectordb::SegmentRow b;
        b.id = 55;
        b.values = {1.0f, 0.0f};
        ASSERT_EQ(writer.append(b), vectordb::Status::ok);
        ASSERT_EQ(writer.finish(), vectordb::Status::ok);
    }

    vectordb::SegmentReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    std::vector<vectordb::SegmentRow> rows;
    ASSERT_EQ(reader.read_all(rows), vectordb::Status::ok);
    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].id, 101u);
    EXPECT_FALSE(rows[0].is_deleted);
    ASSERT_EQ(rows[0].values.size(), 2u);
    EXPECT_FLOAT_EQ(rows[0].values[0], 0.1f);
    EXPECT_FLOAT_EQ(rows[0].values[1], 0.2f);

    EXPECT_EQ(rows[1].id, 55u);
    EXPECT_FALSE(rows[1].is_deleted);
    ASSERT_EQ(rows[1].values.size(), 2u);
    EXPECT_FLOAT_EQ(rows[1].values[0], 1.0f);
    EXPECT_FLOAT_EQ(rows[1].values[1], 0.0f);
}

TEST_F(SegmentTest, IncludesTombstoneRow) {
    {
        vectordb::SegmentWriter writer(path_);
        ASSERT_EQ(writer.open(2, vectordb::Metric::euclidean), vectordb::Status::ok);

        vectordb::SegmentRow live;
        live.id = 1;
        live.values = {3.0f, 4.0f};
        ASSERT_EQ(writer.append(live), vectordb::Status::ok);

        vectordb::SegmentRow tomb;
        tomb.id = 2;
        tomb.is_deleted = true;
        ASSERT_EQ(writer.append(tomb), vectordb::Status::ok);
        ASSERT_EQ(writer.finish(), vectordb::Status::ok);
    }

    vectordb::SegmentReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    std::vector<vectordb::SegmentRow> rows;
    ASSERT_EQ(reader.read_all(rows), vectordb::Status::ok);
    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].id, 1u);
    EXPECT_FALSE(rows[0].is_deleted);
    ASSERT_EQ(rows[0].values.size(), 2u);

    EXPECT_EQ(rows[1].id, 2u);
    EXPECT_TRUE(rows[1].is_deleted);
    EXPECT_TRUE(rows[1].values.empty());
}

TEST_F(SegmentTest, RejectsLiveRowDimensionMismatch) {
    vectordb::SegmentWriter writer(path_);
    ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);

    vectordb::SegmentRow bad;
    bad.id = 1;
    bad.values = {1.0f};
    EXPECT_EQ(writer.append(bad), vectordb::Status::invalid_argument);
}

TEST_F(SegmentTest, RejectsTombstoneWithValues) {
    vectordb::SegmentWriter writer(path_);
    ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);

    vectordb::SegmentRow bad;
    bad.id = 1;
    bad.is_deleted = true;
    bad.values = {1.0f, 2.0f};
    EXPECT_EQ(writer.append(bad), vectordb::Status::invalid_argument);
}

TEST_F(SegmentTest, BadMagicFails) {
    {
        vectordb::SegmentWriter writer(path_);
        ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);
        ASSERT_EQ(writer.finish(), vectordb::Status::ok);
    }

    {
        std::fstream file(path_, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.good());
        file.seekp(0);
        const char bad[8] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
        file.write(bad, 8);
    }

    vectordb::SegmentReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    std::vector<vectordb::SegmentRow> rows;
    EXPECT_EQ(reader.read_all(rows), vectordb::Status::invalid_argument);
}

TEST_F(SegmentTest, BadChecksumFails) {
    {
        vectordb::SegmentWriter writer(path_);
        ASSERT_EQ(writer.open(2, vectordb::Metric::cosine), vectordb::Status::ok);
        vectordb::SegmentRow a;
        a.id = 1;
        a.values = {1.0f, 2.0f};
        ASSERT_EQ(writer.append(a), vectordb::Status::ok);
        ASSERT_EQ(writer.finish(), vectordb::Status::ok);
    }

    {
        std::fstream file(path_, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.good());
        file.seekp(-1, std::ios::end);
        char b = 0;
        file.read(&b, 1);
        file.seekp(-1, std::ios::end);
        b = static_cast<char>(b + 1);
        file.write(&b, 1);
    }

    vectordb::SegmentReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    std::vector<vectordb::SegmentRow> rows;
    EXPECT_EQ(reader.read_all(rows), vectordb::Status::invalid_argument);
}

TEST_F(SegmentTest, MissingFileOpenFails) {
    const auto missing = (dir_ / "nope.vec").string();
    vectordb::SegmentReader reader(missing);
    EXPECT_EQ(reader.open(), vectordb::Status::invalid_argument);
}
