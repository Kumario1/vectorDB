#include "vectordb/database.hpp"
#include "vectordb/serializer.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

class PersistenceSaveTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_persist_test";
        fs::create_directories(dir_);
        path_ = (dir_ / "test.vdb").string();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    static std::uintmax_t file_size(const std::string& path) {
        return fs::file_size(path);
    }

    // 36 header + 8*n + 1*n + 4*n*d + 4 checksum
    static std::uintmax_t expected_size(std::size_t n, std::size_t d) {
        return 36 + 8 * n + 1 * n + 4 * n * d + 4;
    }

    fs::path dir_;
    std::string path_;
};

TEST_F(PersistenceSaveTest, EmptyDatabaseIsFortyBytes) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::save_database(db, path_), vectordb::Status::ok);
    EXPECT_EQ(file_size(path_), expected_size(0, 2));
    EXPECT_EQ(file_size(path_), 40u);
}

TEST_F(PersistenceSaveTest, OneVectorMatchesFormula) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const float v[] = {1.0f, 2.0f};
    ASSERT_EQ(db.insert(1, v), vectordb::Status::ok);
    ASSERT_EQ(vectordb::save_database(db, path_), vectordb::Status::ok);
    // n=1, d=2 → 36 + 8 + 1 + 8 + 4 = 57
    EXPECT_EQ(file_size(path_), expected_size(1, 2));
    EXPECT_EQ(file_size(path_), 57u);
}

TEST_F(PersistenceSaveTest, IncludesTombstonesInPhysicalCount) {
    vectordb::VectorDB db(3, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const float a[] = {1.0f, 0.0f, 0.0f};
    const float b[] = {0.0f, 1.0f, 0.0f};
    ASSERT_EQ(db.insert(10, a), vectordb::Status::ok);
    ASSERT_EQ(db.insert(20, b), vectordb::Status::ok);
    ASSERT_EQ(db.remove(10), vectordb::Status::ok);
    ASSERT_EQ(db.size(), 1u);
    ASSERT_EQ(db.physical_size(), 2u);

    ASSERT_EQ(vectordb::save_database(db, path_), vectordb::Status::ok);
    // physical n=2 still on disk
    EXPECT_EQ(file_size(path_), expected_size(2, 3));
}

TEST_F(PersistenceSaveTest, BadPathReturnsInvalidArgument) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const auto bad = (dir_ / "no_such_dir" / "x.vdb").string();
    EXPECT_EQ(vectordb::save_database(db, bad), vectordb::Status::invalid_argument);
}

TEST_F(PersistenceSaveTest, StartsWithMagic) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::save_database(db, path_), vectordb::Status::ok);

    std::ifstream in(path_, std::ios::binary);
    char magic[8]{};
    in.read(magic, 8);
    ASSERT_TRUE(in.good());
    EXPECT_EQ(std::string(magic, 8), "VECDB001");
}

class PersistenceRoundTripTest : public PersistenceSaveTest {};

TEST_F(PersistenceRoundTripTest, EmptyRoundTrip) {
    vectordb::VectorDB original(4, vectordb::Metric::dot_product, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::save_database(original, path_), vectordb::Status::ok);

    vectordb::VectorDB loaded(1, vectordb::Metric::cosine, vectordb::StorageMode::legacy);  // wrong dims on purpose — load must replace
    ASSERT_EQ(vectordb::load_database(path_, loaded), vectordb::Status::ok);
    EXPECT_EQ(loaded.dimensions(), 4u);
    EXPECT_EQ(loaded.metric(), vectordb::Metric::dot_product);
    EXPECT_EQ(loaded.size(), 0u);
    EXPECT_EQ(loaded.physical_size(), 0u);
}

TEST_F(PersistenceRoundTripTest, VectorsAndSearchSurvive) {
    vectordb::VectorDB original(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    const float c[] = {0.7f, 0.7f};
    ASSERT_EQ(original.insert(1, a), vectordb::Status::ok);
    ASSERT_EQ(original.insert(2, b), vectordb::Status::ok);
    ASSERT_EQ(original.insert(3, c), vectordb::Status::ok);
    ASSERT_EQ(vectordb::save_database(original, path_), vectordb::Status::ok);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::load_database(path_, loaded), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 3u);
    EXPECT_EQ(loaded.physical_size(), 3u);

    auto ga = loaded.get(1);
    auto gb = loaded.get(2);
    auto gc = loaded.get(3);
    ASSERT_TRUE(ga.has_value());
    ASSERT_TRUE(gb.has_value());
    ASSERT_TRUE(gc.has_value());
    EXPECT_EQ((*ga)[0], 1.0f);
    EXPECT_EQ((*ga)[1], 0.0f);
    EXPECT_EQ((*gb)[0], 0.0f);
    EXPECT_EQ((*gb)[1], 1.0f);

    const float query[] = {1.0f, 0.0f};
    auto hits = loaded.search(query, 2);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].id, 1u);
}

TEST_F(PersistenceRoundTripTest, TombstonesPreserved) {
    vectordb::VectorDB original(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    ASSERT_EQ(original.insert(10, a), vectordb::Status::ok);
    ASSERT_EQ(original.insert(20, b), vectordb::Status::ok);
    ASSERT_EQ(original.remove(10), vectordb::Status::ok);
    ASSERT_EQ(vectordb::save_database(original, path_), vectordb::Status::ok);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::load_database(path_, loaded), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded.physical_size(), 2u);
    EXPECT_FALSE(loaded.get(10).has_value());
    ASSERT_TRUE(loaded.get(20).has_value());
    EXPECT_EQ((*loaded.get(20))[1], 1.0f);
    EXPECT_TRUE(loaded.is_deleted_at(0));
    EXPECT_FALSE(loaded.is_deleted_at(1));
}

TEST_F(PersistenceRoundTripTest, EuclideanMetricPreserved) {
    vectordb::VectorDB original(2, vectordb::Metric::euclidean, vectordb::StorageMode::legacy);
    const float v[] = {3.0f, 4.0f};
    ASSERT_EQ(original.insert(7, v), vectordb::Status::ok);
    ASSERT_EQ(vectordb::save_database(original, path_), vectordb::Status::ok);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(vectordb::load_database(path_, loaded), vectordb::Status::ok);
    EXPECT_EQ(loaded.metric(), vectordb::Metric::euclidean);
}

class PersistenceCorruptionTest : public PersistenceSaveTest {
protected:
    void write_valid_one_vector() {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        const float v[] = {1.0f, 2.0f};
        ASSERT_EQ(db.insert(1, v), vectordb::Status::ok);
        ASSERT_EQ(vectordb::save_database(db, path_), vectordb::Status::ok);
    }
};

TEST_F(PersistenceCorruptionTest, MissingFileFails) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    const auto missing = (dir_ / "missing.vdb").string();
    EXPECT_EQ(vectordb::load_database(missing, db), vectordb::Status::invalid_argument);
}

TEST_F(PersistenceCorruptionTest, BadMagicFails) {
    write_valid_one_vector();
    {
        std::fstream f(path_, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(f.good());
        f.seekp(0);
        f.write("BADMAGIC", 8);
    }
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    EXPECT_EQ(vectordb::load_database(path_, db), vectordb::Status::invalid_argument);
}

TEST_F(PersistenceCorruptionTest, TruncatedFileFails) {
    write_valid_one_vector();
    const auto size = fs::file_size(path_);
    ASSERT_GT(size, 10u);
    fs::resize_file(path_, size / 2);

    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    EXPECT_EQ(vectordb::load_database(path_, db), vectordb::Status::invalid_argument);
}

TEST_F(PersistenceCorruptionTest, BadChecksumFails) {
    write_valid_one_vector();
    const auto size = static_cast<std::streamoff>(fs::file_size(path_));
    {
        std::fstream f(path_, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(f.good());
        f.seekp(size - 4);
        const std::uint32_t junk = 0xDEADBEEFu;
        f.write(reinterpret_cast<const char*>(&junk), 4);
    }
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    EXPECT_EQ(vectordb::load_database(path_, db), vectordb::Status::invalid_argument);
}

TEST_F(PersistenceCorruptionTest, WrongVersionFails) {
    write_valid_one_vector();
    {
        std::fstream f(path_, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(f.good());
        f.seekp(8);  // after magic
        const std::uint32_t bad_version = 99;
        f.write(reinterpret_cast<const char*>(&bad_version), 4);
    }
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    EXPECT_EQ(vectordb::load_database(path_, db), vectordb::Status::invalid_argument);
}
