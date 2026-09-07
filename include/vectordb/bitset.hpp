#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vectordb {

// Dynamic bit array packed into 64-bit words.
// M9.2: set/clear/test. M9.3 (#23): combine + iterate set bits.
class Bitset {
public:
    static constexpr std::size_t kBitsPerWord = 64;

    void set(std::size_t i);
    void clear(std::size_t i);
    bool test(std::size_t i) const;

    // Number of bit positions [0, size()). Grows on set(); does not shrink on clear().
    std::size_t size() const noexcept { return bit_size_; }

    // --- M9.3: you implement these ---

    // Slot is 1 only if BOTH have 1. Missing words act as 0.
    // Result size = min(this->size(), other.size()).
    Bitset operator&(const Bitset& other) const;

    // Slot is 1 if EITHER has 1. Missing words act as 0.
    // Result size = max(this->size(), other.size()).
    Bitset operator|(const Bitset& other) const;

    // Slot is 1 if the two bits DIFFER. Missing words act as 0.
    // Result size = max(this->size(), other.size()).
    Bitset operator^(const Bitset& other) const;

    // Flip every bit in [0, size()). Do NOT leave junk 1-bits past size()
    // in the last word (mask the unused tail).
    Bitset operator~() const;

    // How many 1-bits in [0, size()). Use std::popcount; mask last word.
    std::size_t count() const;

    // Ascending list of positions that are 1. Skip empty words;
    // use std::countr_zero + (word &= word - 1). Do not scan with test().
    std::vector<std::size_t> set_bits() const;

private:
    void ensure_size(std::size_t i);

    static std::size_t word_index(std::size_t i) noexcept { return i / kBitsPerWord; }
    static std::size_t bit_offset(std::size_t i) noexcept { return i % kBitsPerWord; }
    static std::uint64_t bit_mask(std::size_t i) noexcept {
        return std::uint64_t{1} << bit_offset(i);
    }

    // Hint for M9.3: bits used in the last word when size is not a multiple of 64.
    // Example: size=8 → mask with low 8 bits set = (1ULL << 8) - 1.
    // size multiple of 64 → all bits of the last word are live (~0ULL).
    // static std::uint64_t last_word_mask(std::size_t bit_size);

    std::vector<std::uint64_t> words_;
    std::size_t bit_size_ = 0;
};

}  // namespace vectordb
