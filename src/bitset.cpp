#include "vectordb/bitset.hpp"

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

}  // namespace vectordb
