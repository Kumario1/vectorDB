#include "vectordb/bitset.hpp"
#include <bit>   // std::popcount, std::countr_zero — you'll need this

#include <algorithm>

namespace vectordb {

void Bitset::ensure_size(std::size_t i) {
    const std::size_t needed = i + 1;
    if (needed <= bit_size_) {
        return;
    }
    words_.resize(word_index(needed - 1) + 1, 0);
    bit_size_ = needed;
}

void Bitset::set(std::size_t i) {
    ensure_size(i);
    words_[word_index(i)] |= bit_mask(i);
}

void Bitset::clear(std::size_t i) {
    if (i >= bit_size_) {
        return;
    }
    words_[word_index(i)] &= ~bit_mask(i);
}

bool Bitset::test(std::size_t i) const {
    if (i >= bit_size_) {
        return false;
    }
    return (words_[word_index(i)] & bit_mask(i)) != 0;
}

// ---------------------------------------------------------------------------
// M9.3 stubs — replace each body. Keep the signatures.
// ---------------------------------------------------------------------------

Bitset Bitset::operator&(const Bitset& other) const {
    // TODO:
    // 1. result.bit_size_ = min(bit_size_, other.bit_size_)
    // 2. resize result.words_ to cover that size
    // 3. for each word i: result.words_[i] = words_[i] & other.words_[i]
    //    (if one side has fewer words, treat missing as 0 — AND is already 0)

    std::size_t bit_size = std::min(bit_size_, other.bit_size_);
    Bitset result;
    result.bit_size_ = bit_size;
    if (result.bit_size_ == 0) {
        return result;
    }

    result.words_.resize(word_index(bit_size - 1) + 1);

    for (size_t i = 0; i < result.words_.size(); i++) {
        result.words_[i] = words_[i] & other.words_[i];
    }

    //cleanup
    std::size_t live = result.bit_size_ % 64;
    if (live != 0) {
        result.words_[result.words_.size() - 1] &= (1ULL << live) - 1;
    }

    return result;
}

Bitset Bitset::operator|(const Bitset& other) const {
    // TODO:
    // 1. result.bit_size_ = max(bit_size_, other.bit_size_)
    // 2. for each word: OR the two sides (missing word = 0)
    std::size_t bit_size = std::max(bit_size_, other.bit_size_);
    Bitset result;
    result.bit_size_ = bit_size;
    if (result.bit_size_ == 0) {
        return result;
    }

    result.words_.resize(word_index(bit_size - 1) + 1);

    for (size_t i = 0; i < result.words_.size(); i++) {
        //we have to check which one is shorter
        if (i < words_.size() && i < other.words_.size()) {
            result.words_[i] = words_[i] | other.words_[i];
        } else if (i < words_.size()) {
            result.words_[i] = words_[i];
        } else if (i < other.words_.size()) {
            result.words_[i] = other.words_[i];
        }
    }

    //cleanup
    std::size_t live = result.bit_size_ % 64;
    if (live != 0) {
        result.words_[result.words_.size() - 1] &= (1ULL << live) - 1;
    }

    return result;
}

Bitset Bitset::operator^(const Bitset& other) const {
    // TODO: like OR for size (max); word op is ^
    Bitset result;
    std::size_t bit_size = std::max(bit_size_, other.bit_size_);
    result.bit_size_ = bit_size;
    if (result.bit_size_ == 0) {
        return result;
    }

    result.words_.resize(word_index(bit_size - 1) + 1);

    for (size_t i = 0; i < result.words_.size(); i++) {
        //we have to check which one is shorter
        if (i < words_.size() && i < other.words_.size()) {
            result.words_[i] = words_[i] ^ other.words_[i];
        } else if (i < words_.size()) {
            result.words_[i] = words_[i];
        } else if (i < other.words_.size()) {
            result.words_[i] = other.words_[i];
        }
    }

    //cleanup
    std::size_t live = result.bit_size_ % 64;
    if (live != 0) {
        result.words_[result.words_.size() - 1] &= (1ULL << live) - 1;
    }

    return result;
}

Bitset Bitset::operator~() const {
    Bitset result;
    result.bit_size_ = bit_size_;
    if (result.bit_size_ == 0) {
        return result;
    }

    result.words_.resize(words_.size());
    for (std::size_t i = 0; i < words_.size(); ++i) {
        result.words_[i] = ~words_[i];
    }

    // Clear unused bits past size() in the last word (same live rule as count).
    std::size_t live = result.bit_size_ % 64;
    if (live != 0) {
        result.words_.back() &= (1ULL << live) - 1;
    }

    return result;
}

std::size_t Bitset::count() const {
    std::size_t total = 0;
    for (size_t w = 0; w < words_.size(); w++) {
        std::uint64_t word = words_[w];
        std::size_t live = bit_size_ % 64;
        if (w == words_.size() - 1 && live != 0) {
            word &= (1ULL << live) - 1;
        } 
        total += std::popcount(word);
    }
    return total;
}

std::vector<std::size_t> Bitset::set_bits() const {
    std::vector<std::size_t> result;
    for (size_t w = 0; w < words_.size(); w++) {
        std::uint64_t word = words_[w];
        if (word == 0) continue;
        while (word != 0) {
            std::size_t bit = std::countr_zero(word);
            std::size_t slot = w * 64 + bit;
            if (slot < bit_size_) result.push_back(slot);
            word &= word - 1;
        }
    }
    return result;
}

}  // namespace vectordb
