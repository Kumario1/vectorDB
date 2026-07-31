#include "vectordb/distance.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using vectordb::cosine_similarity;
using vectordb::dot_product;
using vectordb::squared_euclidean;

TEST(DistanceTest, DotProductIdentical) {
    const float a[] = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(dot_product(a, a), 14.0f);  // 1+4+9
}

TEST(DistanceTest, DotProductOrthogonal) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    EXPECT_FLOAT_EQ(dot_product(a, b), 0.0f);
}

TEST(DistanceTest, DotProductOpposite) {
    const float a[] = {1.0f, 2.0f};
    const float b[] = {-1.0f, -2.0f};
    EXPECT_FLOAT_EQ(dot_product(a, b), -5.0f);
}

TEST(DistanceTest, DotProductHandCalculated) {
    const float a[] = {2.0f, 3.0f};
    const float b[] = {4.0f, 5.0f};
    EXPECT_FLOAT_EQ(dot_product(a, b), 23.0f);  // 8 + 15
}

TEST(DistanceTest, CosineIdenticalIsOne) {
    const float a[] = {3.0f, 4.0f};
    EXPECT_NEAR(cosine_similarity(a, a), 1.0f, 1e-6f);
}

TEST(DistanceTest, CosineOrthogonalIsZero) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    EXPECT_NEAR(cosine_similarity(a, b), 0.0f, 1e-6f);
}

TEST(DistanceTest, CosineOppositeIsNegOne) {
    const float a[] = {1.0f, 2.0f, 3.0f};
    const float b[] = {-1.0f, -2.0f, -3.0f};
    EXPECT_NEAR(cosine_similarity(a, b), -1.0f, 1e-6f);
}

TEST(DistanceTest, CosineHandCalculated) {
    // a = (3,4), b = (4,3)
    // dot = 24, ||a||=5, ||b||=5 → cosine = 24/25 = 0.96
    const float a[] = {3.0f, 4.0f};
    const float b[] = {4.0f, 3.0f};
    EXPECT_NEAR(cosine_similarity(a, b), 0.96f, 1e-6f);
}

TEST(DistanceTest, CosineZeroVectorReturnsZero) {
    const float zero[] = {0.0f, 0.0f, 0.0f};
    const float a[] = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(cosine_similarity(zero, a), 0.0f);
    EXPECT_FLOAT_EQ(cosine_similarity(a, zero), 0.0f);
    EXPECT_FLOAT_EQ(cosine_similarity(zero, zero), 0.0f);
}

TEST(DistanceTest, SquaredEuclideanIdenticalIsZero) {
    const float a[] = {1.0f, -2.0f, 3.5f};
    EXPECT_FLOAT_EQ(squared_euclidean(a, a), 0.0f);
}

TEST(DistanceTest, SquaredEuclideanHandCalculated) {
    // (1,2) vs (4,6): diffs (-3,-4) → 9+16 = 25
    const float a[] = {1.0f, 2.0f};
    const float b[] = {4.0f, 6.0f};
    EXPECT_FLOAT_EQ(squared_euclidean(a, b), 25.0f);
}

TEST(DistanceTest, SquaredEuclideanSkipsSqrt) {
    // Distance would be 5; squared is 25 — we return 25.
    const float a[] = {0.0f, 0.0f};
    const float b[] = {3.0f, 4.0f};
    EXPECT_FLOAT_EQ(squared_euclidean(a, b), 25.0f);
    EXPECT_FLOAT_EQ(std::sqrt(squared_euclidean(a, b)), 5.0f);
}

TEST(DistanceTest, LargeAndSmallValues) {
    const float a[] = {1e6f, 1e-6f};
    const float b[] = {1e6f, 0.0f};
    EXPECT_NEAR(dot_product(a, b), 1e12f, 1e6f);  // allow float noise on huge product
    EXPECT_GE(squared_euclidean(a, b), 0.0f);
    EXPECT_NEAR(cosine_similarity(a, a), 1.0f, 1e-5f);
}

TEST(DistanceTest, EmptyVectors) {
    std::span<const float> empty{};
    EXPECT_FLOAT_EQ(dot_product(empty, empty), 0.0f);
    EXPECT_FLOAT_EQ(squared_euclidean(empty, empty), 0.0f);
    EXPECT_FLOAT_EQ(cosine_similarity(empty, empty), 0.0f);  // zero norms → 0
}
