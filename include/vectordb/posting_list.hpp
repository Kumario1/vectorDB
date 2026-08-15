#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vectordb {

// Sorted, deduplicated list of vector ids for one (field, value) posting.
// M8.3 — standalone; inverted index wires this in #19.
class PostingList {
public:
    void insert(std::uint64_t id);   // keep sorted; no-op if already present
    bool erase(std::uint64_t id);    // true if removed
    bool contains(std::uint64_t id) const;

    const std::vector<std::uint64_t>& ids() const noexcept { return ids_; }
    std::size_t size() const noexcept { return ids_.size(); }
    bool empty() const noexcept { return ids_.empty(); }

private:
    std::vector<std::uint64_t> ids_;  // ascending

    friend PostingList intersect(const PostingList& a, const PostingList& b);
};

// Two-pointer intersection. Result is sorted + unique.
PostingList intersect(const PostingList& a, const PostingList& b);

// Fold pairwise; tip: intersect smallest lists first (fewer comparisons).
PostingList intersect_all(const std::vector<PostingList>& lists);

}  // namespace vectordb
