#include "vectordb/flat_vector_store.hpp"

#include <stdexcept>

namespace vectordb {

FlatVectorStore::FlatVectorStore(std::size_t dimensions) : dimensions_(dimensions) {}

std::optional<std::size_t> FlatVectorStore::append(std::uint64_t id,std::span<const float> values) {
    if (values.size() != dimensions_) return std::nullopt;
    const std::size_t position = ids_.size();
    ids_.push_back(id);
    deleted_.push_back(false);
    values_.insert(values_.end(), values.begin(), values.end());
    return position;
    // 1) if values.size() != dimensions_ → return nullopt
    // 2) position = ids_.size()
    // 3) ids_.push_back(id)
    // 4) deleted_.push_back(false)
    // 5) append the floats onto values_ (insert at end, or loop push_back)
    // 6) return position
}

std::uint64_t FlatVectorStore::id_at(std::size_t position) const {
    // return ids_.at(position)
    return ids_.at(position);
}

std::span<const float> FlatVectorStore::values_at(std::size_t position) const {
    // 1) if position >= ids_.size() → throw std::out_of_range
    if (position >= ids_.size()) throw std::out_of_range("position out of range");
    // 2) start = position * dimensions_
    const std::size_t start = position * dimensions_;
    // 3) return span from values_.data() + start, length dimensions_
    return std::span<const float>(values_.data() + start, dimensions_);
}

std::span<float> FlatVectorStore::values_at(std::size_t position) {
    // same as const version, but span<float>
    if (position >= ids_.size()) throw std::out_of_range("position out of range");
    const std::size_t start = position * dimensions_;
    return std::span<float>(values_.data() + start, dimensions_);
}

bool FlatVectorStore::is_deleted(std::size_t position) const {
    // return deleted_.at(position)
    return deleted_.at(position);
}

void FlatVectorStore::set_deleted(std::size_t position, bool deleted) {
    // deleted_.at(position) = deleted
    deleted_.at(position) = deleted;
}

std::size_t FlatVectorStore::size() const noexcept {
    // return ids_.size()
    return ids_.size();
}

std::size_t FlatVectorStore::dimensions() const noexcept {
    // return dimensions_
    return dimensions_;
}

}  // namespace vectordb