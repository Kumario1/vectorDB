#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace vectordb {

// Version B: structure-of-arrays, floats in one contiguous buffer.
class FlatVectorStore {
public:
    explicit FlatVectorStore(std::size_t dimensions);

    // Appends d floats. Returns position, or empty on dimension mismatch.
    std::optional<std::size_t> append(std::uint64_t id, std::span<const float> values);

    std::uint64_t id_at(std::size_t position) const;
    std::span<const float> values_at(std::size_t position) const;
    std::span<float> values_at(std::size_t position);

    bool is_deleted(std::size_t position) const;
    void set_deleted(std::size_t position, bool deleted);

    std::size_t size() const noexcept;        // number of rows (== ids_.size())
    std::size_t dimensions() const noexcept;

private:
    std::size_t dimensions_;
    std::vector<float> values_;
    std::vector<std::uint64_t> ids_;
    std::vector<bool> deleted_;
};

}  // namespace vectordb