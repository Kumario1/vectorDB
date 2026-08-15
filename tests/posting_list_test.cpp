#include "vectordb/posting_list.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

using vectordb::PostingList;
using vectordb::intersect;
using vectordb::intersect_all;

namespace {

PostingList make_list(std::initializer_list<std::uint64_t> ids) {
    PostingList pl;
    for (std::uint64_t id : ids) {
        pl.insert(id);
    }
    return pl;
}

std::vector<std::uint64_t> set_intersect(std::vector<std::uint64_t> a,
                                         std::vector<std::uint64_t> b) {
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    a.erase(std::unique(a.begin(), a.end()), a.end());
    b.erase(std::unique(b.begin(), b.end()), b.end());
    std::vector<std::uint64_t> out;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                          std::back_inserter(out));
    return out;
}

}  // namespace

TEST(PostingListTest, InsertKeepsSortedAndDeduped) {
    PostingList pl;
    pl.insert(5);
    pl.insert(1);
    pl.insert(3);
    pl.insert(1);  // duplicate
    ASSERT_EQ(pl.ids(), (std::vector<std::uint64_t>{1, 3, 5}));
    EXPECT_EQ(pl.size(), 3u);
}

TEST(PostingListTest, EraseAndContains) {
    PostingList pl = make_list({1, 2, 3});
    EXPECT_TRUE(pl.contains(2));
    EXPECT_TRUE(pl.erase(2));
    EXPECT_FALSE(pl.contains(2));
    EXPECT_FALSE(pl.erase(2));
    EXPECT_EQ(pl.ids(), (std::vector<std::uint64_t>{1, 3}));
}

TEST(PostingListTest, IntersectOverlapping) {
    auto a = make_list({1, 3, 5, 7});
    auto b = make_list({3, 4, 5, 9});
    EXPECT_EQ(intersect(a, b).ids(), (std::vector<std::uint64_t>{3, 5}));
}

TEST(PostingListTest, IntersectDisjoint) {
    auto a = make_list({1, 2});
    auto b = make_list({8, 9});
    EXPECT_TRUE(intersect(a, b).empty());
}

TEST(PostingListTest, IntersectIdentical) {
    auto a = make_list({1, 2, 3});
    auto b = make_list({1, 2, 3});
    EXPECT_EQ(intersect(a, b).ids(), a.ids());
}

TEST(PostingListTest, IntersectEmpty) {
    PostingList empty;
    auto a = make_list({1, 2});
    EXPECT_TRUE(intersect(a, empty).empty());
    EXPECT_TRUE(intersect(empty, a).empty());
    EXPECT_TRUE(intersect(empty, empty).empty());
}

TEST(PostingListTest, IntersectAllThree) {
    std::vector<PostingList> lists{
        make_list({1, 2, 3, 4}),
        make_list({2, 3, 4, 5}),
        make_list({0, 2, 4}),
    };
    EXPECT_EQ(intersect_all(lists).ids(), (std::vector<std::uint64_t>{2, 4}));
}

TEST(PostingListTest, IntersectAllEmptyInput) {
    EXPECT_TRUE(intersect_all({}).empty());
}

TEST(PostingListTest, IntersectMatchesStdSetIntersection) {
    const std::vector<std::uint64_t> left = {9, 1, 4, 4, 7, 2};
    const std::vector<std::uint64_t> right = {7, 0, 4, 8, 1};
    auto a = make_list({});
    auto b = make_list({});
    for (auto id : left) {
        a.insert(id);
    }
    for (auto id : right) {
        b.insert(id);
    }
    EXPECT_EQ(intersect(a, b).ids(), set_intersect(left, right));
}
