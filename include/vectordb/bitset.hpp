#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vectordb {

// Dynamic bit array packed into 64-bit words. M9.2 — standalone; combine ops in #23.
class Bitset {
public:
    static constexpr std::size_t kBitsPerWord = 64;

    void set(std::size_t i);
    void clear(std::size_t i);
    bool test(std::size_t i) const;

    // Number of bit positions [0, size()). Grows on set(); does not shrink on clear().
    std::size_t size() const noexcept { return bit_size_; }

private:
    void ensure_size(std::size_t i);

    static std::size_t word_index(std::size_t i) noexcept { return i / kBitsPerWord; }
    static std::size_t bit_offset(std::size_t i) noexcept { return i % kBitsPerWord; }
    static std::uint64_t bit_mask(std::size_t i) noexcept {
        return std::uint64_t{1} << bit_offset(i);
    }

    std::vector<std::uint64_t> words_;
    std::size_t bit_size_ = 0;
};

}  // namespace vectordb
