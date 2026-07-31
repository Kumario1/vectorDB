#include "vectordb/database.hpp"

#include <gtest/gtest.h>

#include <vector>

using vectordb::Metric;
using vectordb::Status;
using vectordb::VectorDB;

TEST(VectorDBTest, StartsEmpty) {
    VectorDB db(3);
    EXPECT_EQ(db.dimensions(), 3u);
    EXPECT_EQ(db.metric(), Metric::cosine);
    EXPECT_EQ(db.size(), 0u);
    EXPECT_FALSE(db.get(1).has_value());
}

TEST(VectorDBTest, InsertAndGet) {
    VectorDB db(3);
    const float values[] = {1.0f, 2.0f, 3.0f};

    EXPECT_EQ(db.insert(101, values), Status::ok);
    EXPECT_EQ(db.size(), 1u);

    const auto got = db.get(101);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->size(), 3u);
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
    EXPECT_FLOAT_EQ((*got)[1], 2.0f);
    EXPECT_FLOAT_EQ((*got)[2], 3.0f);
}

TEST(VectorDBTest, RejectsWrongDimension) {
    VectorDB db(3);
    const float bad[] = {1.0f, 2.0f};

    EXPECT_EQ(db.insert(1, bad), Status::dimension_mismatch);
    EXPECT_EQ(db.size(), 0u);
    EXPECT_FALSE(db.get(1).has_value());
}

TEST(VectorDBTest, RejectsDuplicateId) {
    VectorDB db(2);
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};

    ASSERT_EQ(db.insert(7, a), Status::ok);
    EXPECT_EQ(db.insert(7, b), Status::duplicate_id);
    EXPECT_EQ(db.size(), 1u);

    const auto got = db.get(7);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 1.0f);
}

TEST(VectorDBTest, UpdateOverwritesValues) {
    VectorDB db(2);
    const float original[] = {1.0f, 2.0f};
    const float updated[] = {9.0f, 8.0f};

    ASSERT_EQ(db.insert(5, original), Status::ok);
    EXPECT_EQ(db.update(5, updated), Status::ok);
    EXPECT_EQ(db.size(), 1u);

    const auto got = db.get(5);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 9.0f);
    EXPECT_FLOAT_EQ((*got)[1], 8.0f);
}

TEST(VectorDBTest, UpdateMissingIsNotFound) {
    VectorDB db(2);
    const float values[] = {1.0f, 2.0f};
    EXPECT_EQ(db.update(99, values), Status::not_found);
}

TEST(VectorDBTest, UpdateWrongDimension) {
    VectorDB db(2);
    const float ok_vals[] = {1.0f, 2.0f};
    const float bad[] = {1.0f};
    ASSERT_EQ(db.insert(1, ok_vals), Status::ok);
    EXPECT_EQ(db.update(1, bad), Status::dimension_mismatch);
}

TEST(VectorDBTest, RemoveThenGetIsEmpty) {
    VectorDB db(2);
    const float values[] = {3.0f, 4.0f};

    ASSERT_EQ(db.insert(10, values), Status::ok);
    EXPECT_EQ(db.remove(10), Status::ok);
    EXPECT_EQ(db.size(), 0u);
    EXPECT_FALSE(db.get(10).has_value());
    EXPECT_EQ(db.remove(10), Status::not_found);
}

TEST(VectorDBTest, ReinsertAfterRemove) {
    VectorDB db(2);
    const float first[] = {1.0f, 1.0f};
    const float second[] = {2.0f, 2.0f};

    ASSERT_EQ(db.insert(3, first), Status::ok);
    ASSERT_EQ(db.remove(3), Status::ok);
    EXPECT_EQ(db.insert(3, second), Status::ok);
    EXPECT_EQ(db.size(), 1u);

    const auto got = db.get(3);
    ASSERT_TRUE(got.has_value());
    EXPECT_FLOAT_EQ((*got)[0], 2.0f);
}

TEST(VectorDBTest, CustomMetricStored) {
    VectorDB db(4, Metric::euclidean);
    EXPECT_EQ(db.metric(), Metric::euclidean);
    EXPECT_EQ(db.dimensions(), 4u);
}
