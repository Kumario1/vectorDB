#pragma once

#include "vectordb/flat_vector_store.hpp"
#include "vectordb/id_index.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace vectordb {

enum class Metric { cosine, dot_product, euclidean };

enum class Status {
    ok,
    duplicate_id,
    not_found,
    dimension_mismatch,
    invalid_argument
};

struct SearchResult {
    std::uint64_t id;
    float score;
};

class VectorDB {
public:
    explicit VectorDB(std::size_t dimensions, Metric metric = Metric::cosine);

    Status insert(std::uint64_t id, std::span<const float> values);
    Status update(std::uint64_t id, std::span<const float> values);
    Status remove(std::uint64_t id);

    std::optional<std::span<const float>> get(std::uint64_t id) const;
    std::vector<SearchResult> search(std::span<const float> query, std::size_t k) const;
    float score_pair(std::span<const float> query, std::span<const float> candidate) const;

    std::size_t dimensions() const noexcept;
    Metric metric() const noexcept;
    std::size_t size() const noexcept;  // active (non-deleted) count

private:
    Metric metric_;
    FlatVectorStore store_;
    IdIndex index_;
    std::size_t active_count_ = 0;  // optional: track live rows without scanning
};

}  // namespace vectordb