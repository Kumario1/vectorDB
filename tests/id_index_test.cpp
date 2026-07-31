#include "vectordb/id_index.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

using vectordb::IdIndex;

TEST(IdIndexTest, StartsEmpty) {
    IdIndex index;
    EXPECT_EQ(index.size(), 0u);
    EXPECT_FALSE(index.find(1).has_value());
    EXPECT_GE(index.load_factor(), 0.0);
}

TEST(IdIndexTest, InsertAndFind) {
    IdIndex index;
    EXPECT_TRUE(index.insert(101, 0));
    EXPECT_EQ(index.size(), 1u);

    const auto pos = index.find(101);
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(*pos, 0u);
}

TEST(IdIndexTest, RejectsDuplicateId) {
    IdIndex index;
    ASSERT_TRUE(index.insert(7, 0));
    EXPECT_FALSE(index.insert(7, 99));  // duplicate — must not overwrite
    EXPECT_EQ(index.size(), 1u);

    const auto pos = index.find(7);
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(*pos, 0u);  // original position kept
}

TEST(IdIndexTest, FindMissingReturnsEmpty) {
    IdIndex index;
    ASSERT_TRUE(index.insert(1, 0));
    EXPECT_FALSE(index.find(2).has_value());
}

TEST(IdIndexTest, EraseRemovesMapping) {
    IdIndex index;
    ASSERT_TRUE(index.insert(42, 3));
    EXPECT_TRUE(index.erase(42));
    EXPECT_EQ(index.size(), 0u);
    EXPECT_FALSE(index.find(42).has_value());
    EXPECT_FALSE(index.erase(42));  // already gone
}

TEST(IdIndexTest, ReinsertAfterErase) {
    IdIndex index;
    ASSERT_TRUE(index.insert(5, 0));
    ASSERT_TRUE(index.erase(5));
    EXPECT_TRUE(index.insert(5, 10));  // same id, new position OK

    const auto pos = index.find(5);
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(*pos, 10u);
}

TEST(IdIndexTest, ManyInsertsAndFinds) {
    IdIndex index;
    constexpr std::size_t kCount = 10'000;
    for (std::size_t i = 0; i < kCount; ++i) {
        ASSERT_TRUE(index.insert(static_cast<std::uint64_t>(i), i));
    }
    EXPECT_EQ(index.size(), kCount);

    for (std::size_t i = 0; i < kCount; ++i) {
        const auto pos = index.find(static_cast<std::uint64_t>(i));
        ASSERT_TRUE(pos.has_value());
        EXPECT_EQ(*pos, i);
    }
    EXPECT_FALSE(index.find(static_cast<std::uint64_t>(kCount)).has_value());
}

TEST(IdIndexTest, MatchesUnorderedMapOnRandomOps) {
    IdIndex index;
    std::unordered_map<std::uint64_t, std::size_t> reference;

    // Deterministic pseudo-random ops (no <random> dependency needed).
    std::uint64_t state = 0xC0FFEEULL;
    auto next = [&] {
        state = state * 6364136223846793005ULL + 1ULL;
        return state;
    };

    for (int step = 0; step < 20'000; ++step) {
        const std::uint64_t id = next() % 500;
        const int op = static_cast<int>(next() % 3);

        if (op == 0) {
            const std::size_t position = static_cast<std::size_t>(next() % 10'000);
            const bool got = index.insert(id, position);
            const bool expected = !reference.contains(id);
            EXPECT_EQ(got, expected) << "insert id=" << id;
            if (expected) {
                reference.emplace(id, position);
            }
        } else if (op == 1) {
            const auto got = index.find(id);
            const auto it = reference.find(id);
            if (it == reference.end()) {
                EXPECT_FALSE(got.has_value()) << "find missing id=" << id;
            } else {
                ASSERT_TRUE(got.has_value()) << "find present id=" << id;
                EXPECT_EQ(*got, it->second);
            }
        } else {
            const bool got = index.erase(id);
            const bool expected = reference.erase(id) > 0;
            EXPECT_EQ(got, expected) << "erase id=" << id;
        }

        EXPECT_EQ(index.size(), reference.size());
    }
}
