#include "vectordb/database.hpp"
#include <algorithm>

namespace vectordb {

VectorDB::VectorDB(std::size_t dimensions, Metric metric)
    : metric_(metric), store_(dimensions) {}

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

}  // namespace vectordb