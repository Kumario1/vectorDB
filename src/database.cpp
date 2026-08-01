#include "vectordb/database.hpp"
#include "vectordb/distance.hpp"
#include "vectordb/serializer.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <span>

namespace vectordb {

VectorDB::VectorDB(std::size_t dimensions, Metric metric)
    : metric_(metric), store_(dimensions) {}

Status VectorDB::save(const std::string& path) const {
    return save_database(*this, path);
}

Status VectorDB::load(const std::string& path) {
    return load_database(path, *this);
}

Status VectorDB::insert(std::uint64_t id, std::span<const float> values) {
    if (values.size() != store_.dimensions()) {return Status::dimension_mismatch;}
    if (index_.find(id) != std::nullopt) {return Status::duplicate_id;}
    auto pos = store_.append(id, values);
    if (!pos) {return Status::dimension_mismatch;}
    index_.insert(id, *pos);
    ++active_count_;
    return Status::ok;
}

Status VectorDB::update(std::uint64_t id, std::span<const float> values) {
    // 1) wrong dims → dimension_mismatch
    // 2) auto pos = index_.find(id); if !pos → not_found
    // 3) if store_.is_deleted(*pos) → not_found  (defensive)
    // 4) copy values into store_.values_at(*pos)  (same length)
    // 5) return ok
    if (values.size() != store_.dimensions()) {return Status::dimension_mismatch;}
    auto pos = index_.find(id);
    if (!pos) {return Status::not_found;}
    if (store_.is_deleted(*pos)) {return Status::not_found;}
    auto dest = store_.values_at(*pos);
    std::copy(values.begin(), values.end(), dest.begin());
    return Status::ok;
}

Status VectorDB::remove(std::uint64_t id) {
    // 1) auto pos = index_.find(id); if !pos → not_found
    // 2) store_.set_deleted(*pos, true)
    // 3) index_.erase(id)
    // 4) --active_count_
    // 5) return ok
    auto pos = index_.find(id);
    if (!pos) {return Status::not_found;}
    if (store_.is_deleted(*pos)) {return Status::not_found;}
    store_.set_deleted(*pos, true);
    index_.erase(id);
    --active_count_;
    return Status::ok;
}

std::optional<std::span<const float>> VectorDB::get(std::uint64_t id) const {
    // 1) find pos in index_; if missing → nullopt
    // 2) if store_.is_deleted(*pos) → nullopt
    // 3) return store_.values_at(*pos)
    auto pos = index_.find(id);
    if (!pos) {return std::nullopt;}
    if (store_.is_deleted(*pos)) {return std::nullopt;}
    return store_.values_at(*pos);
}

std::size_t VectorDB::dimensions() const noexcept {
    // return store_.dimensions()
    return store_.dimensions();
}

Metric VectorDB::metric() const noexcept {
    // return metric_
    return metric_;
}

std::size_t VectorDB::size() const noexcept {
    // return active_count_
    return active_count_;
}

std::size_t VectorDB::physical_size() const noexcept {
    // return store_.size()
    return store_.size();
}

std::uint64_t VectorDB::id_at(std::size_t index) const noexcept {
    return store_.id_at(index);
}

bool VectorDB::is_deleted_at(std::size_t index) const noexcept {
    return store_.is_deleted(index);
}

std::span<const float> VectorDB::values_at(std::size_t index) const noexcept {
    return store_.values_at(index);
}

float VectorDB::score_pair(std::span<const float> query, std::span<const float> candidate) const {
    assert(query.size() == candidate.size());
    switch (metric_) {
        case Metric::cosine: return cosine_similarity(query, candidate);
        case Metric::dot_product: return dot_product(query, candidate);
        case Metric::euclidean: return -squared_euclidean(query, candidate);
    }
}

struct WorseFirst {
    bool operator()(const SearchResult& a, const SearchResult& b) const {
        return a.score > b.score;  // higher score = "less" → lowest score on top
    }
};

std::vector<SearchResult> VectorDB::search(std::span<const float> query, std::size_t k) const {
    if (query.size() != dimensions()) {return {};}
    if (k == 0) {return {};}
    //min heap of all SearchResults by score (worst on top)
    std::priority_queue<SearchResult, std::vector<SearchResult>, WorseFirst> heap;

    for (std::size_t i = 0; i < store_.size(); ++i) {
        if (store_.is_deleted(i)) {continue;}
        float score = score_pair(query, store_.values_at(i));
        heap.push({store_.id_at(i), score});
        if (heap.size() > k) {heap.pop();}
    }

    std::vector<SearchResult> results;
    while (!heap.empty()) {
        results.push_back(heap.top());
        heap.pop();
    }
    
    std::reverse(results.begin(), results.end());
    return results;
    // 1) if query.size() != dimensions() → return {}  (or we add Status later)
    // 2) if k == 0 → return {}
    // 3) min-heap of SearchResult by score (worst on top)
    //    hint: priority_queue with comparator where a.score > b.score means a is "lower priority"
    // 4) for i in [0, store_.size()): skip deleted; score; push; if size>k pop
    // 5) pop all into a vector, reverse so best is first
}

}  // namespace vectordb
