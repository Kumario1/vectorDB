#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace vectordb {

struct VectorRecord {
    std::uint64_t id{};
    std::vector<float> values;
    bool deleted = false;
};

// Version A: one VectorRecord per vector (simple, not cache-optimal yet).
class VectorStore {
public:
    explicit VectorStore(std::size_t dimensions);

    // Appends a vector. Returns its physical position, or empty if wrong dimension.
    std::optional<std::size_t> append(std::uint64_t id, std::span<const float> values);

    const VectorRecord& at(std::size_t position) const;
    VectorRecord& at(std::size_t position);

    std::size_t size() const noexcept;
    std::size_t dimensions() const noexcept;

private:
    std::size_t dimensions_;
    std::vector<VectorRecord> records_;
};

}  // namespace vectordb