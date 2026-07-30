#include "vectordb/vector_store.hpp"

#include <stdexcept>
#include <utility>

namespace vectordb {

VectorStore::VectorStore(std::size_t dimensions) : dimensions_(dimensions) {}

std::optional<std::size_t> VectorStore::append(std::uint64_t id, std::span<const float> values) {
    if (values.size() != dimensions_) return std::nullopt; //if the wrong dimensions, return empty because this is a fixed vector store
    VectorRecord record;
    record.id = id;
    record.values.assign(values.begin(), values.end());
    record.deleted = false; 

    const std::size_t position = records_.size();
    records_.push_back(std::move(record));
    return position;
}

const VectorRecord& VectorStore::at(std::size_t position) const {
    // Use records_.at(position) so out-of-range throws std::out_of_range
    return records_.at(position);
}

VectorRecord& VectorStore::at(std::size_t position) {
    // Same as above, non-const
    return records_.at(position);
}

std::size_t VectorStore::size() const noexcept {
    // return records_.size()
    return records_.size();
}

std::size_t VectorStore::dimensions() const noexcept {
    // return dimensions_
    return dimensions_;
}

}  // namespace vectordb