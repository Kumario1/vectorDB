#include "vectordb/database.hpp"
#include "vectordb/metadata.hpp"

#include <gtest/gtest.h>

#include <string>

using vectordb::Metadata;
using vectordb::Status;
using vectordb::VectorDB;
using vectordb::get_field;

TEST(MetadataTest, InsertWithMetadataRoundTrip) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}, {"year", std::int64_t{2001}}};

    ASSERT_EQ(db.insert(1, v, meta), Status::ok);

    const auto got = db.get_metadata(1);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(get_field<std::string>(*got, "category"), std::string("book"));
    EXPECT_EQ(get_field<std::int64_t>(*got, "year"), 2001);
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
    EXPECT_FALSE(get_field<std::string>(*got, "missing").has_value());
    EXPECT_FALSE(get_field<std::int64_t>(*got, "category").has_value());  // wrong type
    EXPECT_EQ(get_field<bool>(*got, "active"), true);
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
    EXPECT_EQ(get_field<std::string>(*got, "category"), std::string("book"));
}
