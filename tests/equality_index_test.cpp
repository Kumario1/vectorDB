#include "vectordb/equality_index.hpp"
#include "vectordb/database.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using vectordb::EqualityIndex;
using vectordb::Metadata;
using vectordb::MetadataValue;
using vectordb::Status;
using vectordb::VectorDB;

TEST(EqualityIndexTest, LookupUnknownIsEmpty) {
    EqualityIndex idx;
    EXPECT_TRUE(idx.lookup("category", MetadataValue{std::string("book")}).empty());
}

TEST(EqualityIndexTest, AddThenLookup) {
    EqualityIndex idx;
    Metadata meta{{"category", std::string("book")}, {"year", std::int64_t{2001}}};
    idx.add(4, meta);
    idx.add(8, Metadata{{"category", std::string("book")}});
    idx.add(2, Metadata{{"category", std::string("movie")}});

    EXPECT_EQ(idx.lookup("category", MetadataValue{std::string("book")}).ids(),
              (std::vector<std::uint64_t>{4, 8}));
    EXPECT_EQ(idx.lookup("category", MetadataValue{std::string("movie")}).ids(),
              (std::vector<std::uint64_t>{2}));
    EXPECT_EQ(idx.lookup("year", MetadataValue{std::int64_t{2001}}).ids(),
              (std::vector<std::uint64_t>{4}));
}

TEST(EqualityIndexTest, RemoveDropsFromLists) {
    EqualityIndex idx;
    Metadata meta{{"category", std::string("book")}};
    idx.add(4, meta);
    idx.add(8, meta);
    idx.remove(4, meta);

    EXPECT_EQ(idx.lookup("category", MetadataValue{std::string("book")}).ids(),
              (std::vector<std::uint64_t>{8}));
}

TEST(EqualityIndexTest, UpdateMovesBetweenLists) {
    EqualityIndex idx;
    Metadata old_meta{{"category", std::string("book")}};
    Metadata new_meta{{"category", std::string("movie")}};
    idx.add(4, old_meta);
    idx.update(4, old_meta, new_meta);

    EXPECT_TRUE(idx.lookup("category", MetadataValue{std::string("book")}).empty());
    EXPECT_EQ(idx.lookup("category", MetadataValue{std::string("movie")}).ids(),
              (std::vector<std::uint64_t>{4}));
}

TEST(EqualityIndexTest, DifferentTypesDoNotCollide) {
    EqualityIndex idx;
    idx.add(1, Metadata{{"x", std::int64_t{42}}});
    idx.add(2, Metadata{{"x", std::string("42")}});

    EXPECT_EQ(idx.lookup("x", MetadataValue{std::int64_t{42}}).ids(),
              (std::vector<std::uint64_t>{1}));
    EXPECT_EQ(idx.lookup("x", MetadataValue{std::string("42")}).ids(),
              (std::vector<std::uint64_t>{2}));
}

TEST(EqualityIndexTest, VectorDBInsertRemoveMaintainsIndex) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    Metadata meta{{"category", std::string("book")}};
    ASSERT_EQ(db.insert(4, v, meta), Status::ok);
    ASSERT_EQ(db.insert(8, v, Metadata{{"category", std::string("book")}}), Status::ok);

    EXPECT_EQ(db.lookup("category", MetadataValue{std::string("book")}).ids(),
              (std::vector<std::uint64_t>{4, 8}));

    ASSERT_EQ(db.remove(4), Status::ok);
    EXPECT_EQ(db.lookup("category", MetadataValue{std::string("book")}).ids(),
              (std::vector<std::uint64_t>{8}));
}

TEST(EqualityIndexTest, VectorDBSetMetadataMovesIndex) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    ASSERT_EQ(db.insert(4, v, Metadata{{"category", std::string("book")}}), Status::ok);
    ASSERT_EQ(db.set_metadata(4, Metadata{{"category", std::string("movie")}}), Status::ok);

    EXPECT_TRUE(db.lookup("category", MetadataValue{std::string("book")}).empty());
    EXPECT_EQ(db.lookup("category", MetadataValue{std::string("movie")}).ids(),
              (std::vector<std::uint64_t>{4}));

    EXPECT_EQ(db.set_metadata(99, Metadata{{"category", std::string("book")}}), Status::not_found);
}
