#include "vectordb/id_index.hpp"

namespace vectordb {

bool IdIndex::insert(std::uint64_t id, std::size_t position) {
    if (map_.find(id) != map_.end()) {return false;}
    map_[id] = position;
    return true;
    // 1) if id already in map_ → return false
    // 2) map_[id] = position  (or map_.emplace(id, position))
    // 3) return true
    //
    // Hint: map_.find(id) != map_.end()  OR  map_.contains(id) (C++20)
}

std::optional<std::size_t> IdIndex::find(std::uint64_t id) const {
    // 1) auto it = map_.find(id)
    // 2) if it == map_.end() → return nullopt
    // 3) return it->second

    auto it = map_.find(id);
    if (it == map_.end()) {return std::nullopt;}
    return it->second;
}

bool IdIndex::erase(std::uint64_t id) {
    // map_.erase(id) returns number of elements removed (0 or 1)
    // return true if something was removed
    return map_.erase(id) > 0;
}

std::size_t IdIndex::size() const noexcept {
    // return map_.size()
    return map_.size();
}

double IdIndex::load_factor() const noexcept {
    // if map_.bucket_count() == 0 → return 0.0
    // else return static_cast<double>(map_.size()) / map_.bucket_count()
    if (map_.bucket_count() == 0) {return 0.0;}
    return static_cast<double>(map_.size()) / map_.bucket_count();
}

}  // namespace vectordb