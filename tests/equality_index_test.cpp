// M8.4 starter tests — not wired into CMakeLists yet.
//
// Your job (#19):
//   1. Implement EqualityIndex in src/equality_index.cpp
//   2. Add src/equality_index.cpp to the vectordb library
//   3. Add this file to vector_store_test
//   4. Optional but recommended: hold EqualityIndex on VectorDB and
//      call add/remove/update from insert/remove/set_metadata
//   5. No filtered search yet (#20)
//
#include "vectordb/equality_index.hpp"

#include <gtest/gtest.h>

#include <string>

using vectordb::EqualityIndex;
using vectordb::Metadata;
using vectordb::MetadataValue;

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
