#include "vectordb/database.hpp"
#include "vectordb/distance.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using vectordb::Metric;
using vectordb::SearchResult;
using vectordb::Status;
using vectordb::VectorDB;

TEST(SearchTest, KZeroReturnsEmpty) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    ASSERT_EQ(db.insert(1, v), Status::ok);
    EXPECT_TRUE(db.search(v, 0).empty());
}

TEST(SearchTest, WrongQueryDimensionReturnsEmpty) {
    VectorDB db(2);
    const float v[] = {1.0f, 0.0f};
    ASSERT_EQ(db.insert(1, v), Status::ok);
    const float bad[] = {1.0f};
    EXPECT_TRUE(db.search(bad, 1).empty());
}

TEST(SearchTest, KOneReturnsBestDotProduct) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.0f, 1.0f}), Status::ok);
    ASSERT_EQ(db.insert(3, std::vector<float>{0.9f, 0.1f}), Status::ok);

    const float query[] = {1.0f, 0.0f};
    const auto results = db.search(query, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, 1u);
}

TEST(SearchTest, TopKOrderedBestFirst) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(10, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(20, std::vector<float>{0.5f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(30, std::vector<float>{0.0f, 1.0f}), Status::ok);

    const float query[] = {1.0f, 0.0f};
    const auto results = db.search(query, 2);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].id, 10u);
    EXPECT_EQ(results[1].id, 20u);
    EXPECT_GE(results[0].score, results[1].score);
}

TEST(SearchTest, KGreaterThanSizeReturnsAll) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.0f, 1.0f}), Status::ok);

    const float query[] = {1.0f, 0.0f};
    EXPECT_EQ(db.search(query, 100).size(), 2u);
}

TEST(SearchTest, SkipsDeletedVectors) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.9f, 0.0f}), Status::ok);
    ASSERT_EQ(db.remove(1), Status::ok);

    const float query[] = {1.0f, 0.0f};
    const auto results = db.search(query, 2);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, 2u);
}

TEST(SearchTest, CosineMetric) {
    VectorDB db(2, Metric::cosine);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.0f, 1.0f}), Status::ok);

    const float query[] = {1.0f, 0.0f};
    const auto results = db.search(query, 2);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].id, 1u);
    EXPECT_NEAR(results[0].score, 1.0f, 1e-5f);
    EXPECT_NEAR(results[1].score, 0.0f, 1e-5f);
}

TEST(SearchTest, EuclideanMetricHigherIsBetter) {
    VectorDB db(2, Metric::euclidean);
    ASSERT_EQ(db.insert(1, std::vector<float>{0.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{10.0f, 10.0f}), Status::ok);

    const float query[] = {0.0f, 0.0f};
    const auto results = db.search(query, 2);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].id, 1u);
    EXPECT_GT(results[0].score, results[1].score);
}

TEST(SearchTest, MatchesFullSortReference) {
    VectorDB db(3, Metric::dot_product);
    const std::vector<std::pair<std::uint64_t, std::vector<float>>> rows = {
        {1, {1.0f, 0.0f, 0.0f}},
        {2, {0.0f, 1.0f, 0.0f}},
        {3, {0.5f, 0.5f, 0.0f}},
        {4, {0.2f, 0.1f, 0.7f}},
        {5, {-1.0f, 0.0f, 0.0f}},
    };
    for (const auto& [id, values] : rows) {
        ASSERT_EQ(db.insert(id, values), Status::ok);
    }

    const float query[] = {1.0f, 0.0f, 0.0f};
    constexpr std::size_t k = 3;

    std::vector<SearchResult> expected;
    for (const auto& [id, values] : rows) {
        expected.push_back(SearchResult{id, vectordb::dot_product(query, values)});
    }
    std::sort(expected.begin(), expected.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });
    expected.resize(k);

    const auto got = db.search(query, k);
    ASSERT_EQ(got.size(), k);
    for (std::size_t i = 0; i < k; ++i) {
        EXPECT_EQ(got[i].id, expected[i].id) << "rank " << i;
        EXPECT_FLOAT_EQ(got[i].score, expected[i].score) << "rank " << i;
    }
}
