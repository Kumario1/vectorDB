// M8.2 starter tests — not wired into CMakeLists yet.
//
// Your job (#17):
//   1. Add to VectorDB:
//        Status insert(uint64_t id, span<const float> values, const Metadata& metadata);
//        optional<Metadata> get_metadata(uint64_t id) const;
//      (2-arg insert stays; metadata insert is optional)
//   2. Keep vector + metadata consistent:
//        remove → drop metadata
//        vector-only update → leave metadata
//        reinsert without metadata → clean empty map
//   3. get_metadata: missing/deleted id → nullopt; live id with none → empty Metadata
//   4. Add this file to CMakeLists.txt vector_store_test sources
//   5. Optional helper: get_field<T>(meta, key) → nullopt if missing or wrong type
//
// In-memory only — no segment / .vdb changes.

#include "vectordb/database.hpp"
#include "vectordb/metadata.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

using vectordb::Metadata;
using vectordb::Status;
using vectordb::VectorDB;

namespace {

template <typename T>
std::optional<T> field(const Metadata& meta, const std::string& key) {
    auto it = meta.find(key);
    if (it == meta.end()) {
        return std::nullopt;
    }
    if (const T* p = std::get_if<T>(&it->second)) {
        return *p;
    }
    return std::nullopt;
}

}  // namespace

TEST(MetadataTest, InsertWithMetadataRoundTrip) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}, {"year", std::int64_t{2001}}};

    ASSERT_EQ(db.insert(1, v, meta), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(field<std::string>(*got, "category"), std::string("book"));
    EXPECT_EQ(field<std::int64_t>(*got, "year"), 2001);
}

TEST(MetadataTest, InsertWithoutMetadataReturnsEmptyMap) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    ASSERT_EQ(db.insert(1, v), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->empty());
}

TEST(MetadataTest, GetMetadataMissingIdReturnsEmpty) {
    VectorDB db(2);
    EXPECT_FALSE(db.get_metadata(99).has_value());
}

TEST(MetadataTest, RemoveDropsMetadata) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}};
    ASSERT_EQ(db.insert(1, v, meta), Status::ok);
    ASSERT_EQ(db.remove(1), Status::ok);

    EXPECT_FALSE(db.get(1).has_value());
    EXPECT_FALSE(db.get_metadata(1).has_value());
}

TEST(MetadataTest, ReinsertStartsClean) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}};
    ASSERT_EQ(db.insert(1, v, meta), Status::ok);
    ASSERT_EQ(db.remove(1), Status::ok);
    ASSERT_EQ(db.insert(1, v), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->empty());
}

TEST(MetadataTest, MissingFieldAndWrongType) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}, {"active", true}};
    ASSERT_EQ(db.insert(1, v, meta), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(field<std::string>(*got, "missing").has_value());
    EXPECT_FALSE(field<std::int64_t>(*got, "category").has_value());  // wrong type
    EXPECT_EQ(field<bool>(*got, "active"), true);
}

TEST(MetadataTest, FailedInsertDoesNotStoreMetadata) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}};
    ASSERT_EQ(db.insert(1, v), Status::ok);
    EXPECT_EQ(db.insert(1, v, meta), Status::duplicate_id);
    EXPECT_TRUE(db.get_metadata(1)->empty());
}

TEST(MetadataTest, VectorUpdateLeavesMetadata) {
    VectorDB db(2);
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    Metadata meta{{"category", std::string("book")}};
    ASSERT_EQ(db.insert(1, a, meta), Status::ok);
    ASSERT_EQ(db.update(1, b), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(field<std::string>(*got, "category"), std::string("book"));
}
