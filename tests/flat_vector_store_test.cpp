// FlatVectorStore tests will go here.
#include "vectordb/flat_vector_store.hpp"
#include <vector>
#include <gtest/gtest.h>

//test that starts empty
TEST(FlatVectorStoreTest, StartsEmpty) {
    vectordb::FlatVectorStore store(3);
    EXPECT_EQ(store.dimensions(), 3u);
    EXPECT_EQ(store.size(), 0u);
}

//test that appends one and retrieves it
TEST(FlatVectorStoreTest, AppendOneAndRetrieve) {
    vectordb::FlatVectorStore store(3);
    const float values[] = {1.0f, 2.0f, 3.0f};

    auto pos = store.append(101, values);
    ASSERT_TRUE(pos.has_value());   // append must succeed
    EXPECT_EQ(*pos, 0u);            // first row → position 0
    EXPECT_EQ(store.size(), 1u);

    EXPECT_EQ(store.id_at(0), 101u);
    EXPECT_EQ(store.values_at(0).size(), 3u);
    EXPECT_FLOAT_EQ(store.values_at(0)[0], 1.0f);
    EXPECT_FLOAT_EQ(store.values_at(0)[1], 2.0f);
    EXPECT_FLOAT_EQ(store.values_at(0)[2], 3.0f);
    EXPECT_FALSE(store.is_deleted(0));
}

//test that rejects wrong dimension
TEST(FlatVectorStoreTest, RejectsWrongDimension) {
    vectordb::FlatVectorStore store(3);
    const float bad[] = {1.0f, 2.0f};  // only 2 floats, store wants 3

    auto pos = store.append(1, bad);
    EXPECT_FALSE(pos.has_value());
    EXPECT_EQ(store.size(), 0u);  // store unchanged
}

//tests first and last after several appends
TEST(FlatVectorStoreTest, RetrieveFirstAndLast) {
    vectordb::FlatVectorStore store(2);

    ASSERT_TRUE(store.append(1, std::vector<float>{0.0f, 1.0f}).has_value());
    ASSERT_TRUE(store.append(2, std::vector<float>{2.0f, 3.0f}).has_value());
    ASSERT_TRUE(store.append(3, std::vector<float>{4.0f, 5.0f}).has_value());

    EXPECT_EQ(store.id_at(0), 1u);
    EXPECT_EQ(store.id_at(store.size() - 1), 3u);
    EXPECT_FLOAT_EQ(store.values_at(0)[0], 0.0f);
    EXPECT_FLOAT_EQ(store.values_at(2)[1], 5.0f);
}

//check deleting a record, make sure it flips the deleted flag 
TEST(FlatVectorStoreTest, MarkAndReadTombstone) {
    vectordb::FlatVectorStore store(2);
    ASSERT_TRUE(store.append(7, std::vector<float>{9.0f, 8.0f}).has_value());

    store.set_deleted(0, true);

    EXPECT_TRUE(store.is_deleted(0));
    EXPECT_EQ(store.size(), 1u);  // physical row still there
}

//reallocation tests
TEST(FlatVectorStoreTest, SurvivesManyReallocations) {
    constexpr std::size_t kDims = 4;
    constexpr std::size_t kCount = 10'000;
    vectordb::FlatVectorStore store(kDims);

    for (std::size_t i = 0; i < kCount; ++i) {
        std::vector<float> values(kDims, static_cast<float>(i));
        auto pos = store.append(static_cast<std::uint64_t>(i), values);
        ASSERT_TRUE(pos.has_value());
        EXPECT_EQ(*pos, i);
    }

    EXPECT_EQ(store.size(), kCount);
    EXPECT_EQ(store.id_at(0), 0u);
    EXPECT_EQ(store.id_at(kCount - 1), static_cast<std::uint64_t>(kCount - 1));
    EXPECT_FLOAT_EQ(store.values_at(kCount - 1)[0], static_cast<float>(kCount - 1));
}