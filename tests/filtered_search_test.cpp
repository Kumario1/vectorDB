// M8.5 starter tests — not wired into CMakeLists yet.
//
// Your job (#20):
//   1. Add overload:
//        search(query, k, span<const EqualityPredicate> filter) const
//      Keep existing search(query, k) unchanged (or have it call the new one with empty filter).
//   2. Pre-filter:
//        for each predicate → eq_index_.lookup
//        candidates = intersect_all(those posting lists)
//        score only candidate ids → top-k heap (same as unfiltered)
//   3. Empty / no-match filter → {}
//   4. Wire this file into vector_store_test
//
#include "vectordb/database.hpp"
#include "vectordb/metadata.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using vectordb::EqualityPredicate;
using vectordb::Metadata;
using vectordb::Metric;
using vectordb::SearchResult;
using vectordb::Status;
using vectordb::VectorDB;

namespace {

bool same_ranking(const std::vector<SearchResult>& a, const std::vector<SearchResult>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) {
            return false;
        }
        if (std::fabs(a[i].score - b[i].score) > 1e-5f) {
            return false;
        }
    }
    return true;
}

// Brute-force oracle: unfiltered search with large k, keep ids that match all predicates.
std::vector<SearchResult> post_filter_oracle(VectorDB& db,
                                             std::span<const float> query,
                                             std::size_t k,
                                             const std::vector<EqualityPredicate>& filter) {
    auto all = db.search(query, db.size() + 1);
    std::vector<SearchResult> matched;
    for (const auto& hit : all) {
        auto meta = db.get_metadata(hit.id);
        if (!meta) {
            continue;
        }
        bool ok = true;
        for (const auto& pred : filter) {
            auto it = meta->find(pred.field);
            if (it == meta->end() || it->second != pred.value) {
                ok = false;
                break;
            }
        }
        if (ok) {
            matched.push_back(hit);
        }
    }
    if (matched.size() > k) {
        matched.resize(k);
    }
    return matched;
}

}  // namespace

TEST(FilteredSearchTest, UnfilteredSearchUnchanged) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f}), Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.0f, 1.0f}), Status::ok);
    const float q[] = {1.0f, 0.0f};
    const auto hits = db.search(q, 2);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].id, 1u);
}

TEST(FilteredSearchTest, SingleEqualityFilter) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f},
                        Metadata{{"category", std::string("book")}}),
              Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.9f, 0.0f},
                        Metadata{{"category", std::string("movie")}}),
              Status::ok);
    ASSERT_EQ(db.insert(3, std::vector<float>{0.8f, 0.0f},
                        Metadata{{"category", std::string("book")}}),
              Status::ok);

    const float q[] = {1.0f, 0.0f};
    std::vector<EqualityPredicate> filter{{"category", std::string("book")}};
    const auto hits = db.search(q, 2, filter);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].id, 1u);
    EXPECT_EQ(hits[1].id, 3u);
}

TEST(FilteredSearchTest, MultipleFiltersAnd) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f},
                        Metadata{{"category", std::string("book")},
                                 {"lang", std::string("en")}}),
              Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.95f, 0.0f},
                        Metadata{{"category", std::string("book")},
                                 {"lang", std::string("de")}}),
              Status::ok);
    ASSERT_EQ(db.insert(3, std::vector<float>{0.9f, 0.0f},
                        Metadata{{"category", std::string("movie")},
                                 {"lang", std::string("en")}}),
              Status::ok);

    const float q[] = {1.0f, 0.0f};
    std::vector<EqualityPredicate> filter{
        {"category", std::string("book")},
        {"lang", std::string("en")},
    };
    const auto hits = db.search(q, 5, filter);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].id, 1u);
}

TEST(FilteredSearchTest, MissingFieldOrValueReturnsEmpty) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f},
                        Metadata{{"category", std::string("book")}}),
              Status::ok);
    const float q[] = {1.0f, 0.0f};
    EXPECT_TRUE(db.search(q, 3, std::vector<EqualityPredicate>{{"category", std::string("movie")}})
                    .empty());
    EXPECT_TRUE(db.search(q, 3, std::vector<EqualityPredicate>{{"lang", std::string("en")}}).empty());
}

TEST(FilteredSearchTest, PreFilterMatchesPostFilterOracle) {
    VectorDB db(2, Metric::dot_product);
    ASSERT_EQ(db.insert(1, std::vector<float>{1.0f, 0.0f},
                        Metadata{{"category", std::string("book")}, {"lang", std::string("en")}}),
              Status::ok);
    ASSERT_EQ(db.insert(2, std::vector<float>{0.5f, 0.0f},
                        Metadata{{"category", std::string("book")}, {"lang", std::string("en")}}),
              Status::ok);
    ASSERT_EQ(db.insert(3, std::vector<float>{0.9f, 0.0f},
                        Metadata{{"category", std::string("movie")}, {"lang", std::string("en")}}),
              Status::ok);
    ASSERT_EQ(db.insert(4, std::vector<float>{0.7f, 0.0f},
                        Metadata{{"category", std::string("book")}, {"lang", std::string("de")}}),
              Status::ok);

    const float q[] = {1.0f, 0.0f};
    std::vector<EqualityPredicate> filter{
        {"category", std::string("book")},
        {"lang", std::string("en")},
    };
    const auto pre = db.search(q, 2, filter);
    const auto post = post_filter_oracle(db, q, 2, filter);
    EXPECT_TRUE(same_ranking(pre, post));
}
