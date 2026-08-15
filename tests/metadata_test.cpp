#include "vectordb/database.hpp"
#include "vectordb/metadata.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

using vectordb::Metadata;
using vectordb::Metric;
using vectordb::Status;
using vectordb::StorageMode;
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

TEST(MetadataTest, LegacyModeAlsoStoresAndClearsMetadata) {
    VectorDB db(2, Metric::cosine, StorageMode::legacy);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"lang", std::string("en")}};
    ASSERT_EQ(db.insert(7, v, meta), Status::ok);

    const auto got = db.get_metadata(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(field<std::string>(*got, "lang"), std::string("en"));

    ASSERT_EQ(db.remove(7), Status::ok);
    EXPECT_FALSE(db.get_metadata(7).has_value());
}

TEST(MetadataTest, AllValueTypesRoundTrip) {
    VectorDB db(2);
    const float v[] = {0.0f, 1.0f};
    Metadata meta{
        {"i", std::int64_t{-3}},
        {"d", 2.5},
        {"b", false},
        {"s", std::string("x")},
    };
    ASSERT_EQ(db.insert(3, v, meta), Status::ok);
    const auto got = db.get_metadata(3);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(field<std::int64_t>(*got, "i"), -3);
    EXPECT_DOUBLE_EQ(*field<double>(*got, "d"), 2.5);
    EXPECT_EQ(field<bool>(*got, "b"), false);
    EXPECT_EQ(field<std::string>(*got, "s"), std::string("x"));
}
