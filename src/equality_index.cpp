#include "vectordb/equality_index.hpp"

namespace vectordb {

void EqualityIndex::add(std::uint64_t id, const Metadata& meta) {
    for (const auto& [field, value] : meta) {
        index_[field][value].insert(id);
    }
}

void EqualityIndex::remove(std::uint64_t id, const Metadata& meta) {
    for (const auto& [field, value] : meta) {
        auto fit = index_.find(field);
        if (fit == index_.end()) {
            continue;
        }
        auto vit = fit->second.find(value);
        if (vit == fit->second.end()) {
            continue;
        }
        vit->second.erase(id);
        if (vit->second.empty()) {
            fit->second.erase(vit);
        }
        if (fit->second.empty()) {
            index_.erase(fit);
        }
    }
}

void EqualityIndex::update(std::uint64_t id, const Metadata& old_meta, const Metadata& new_meta) {
    remove(id, old_meta);
    add(id, new_meta);
}

PostingList EqualityIndex::lookup(std::string_view field, const MetadataValue& value) const {
    auto fit = index_.find(std::string(field));
    if (fit == index_.end()) {
        return PostingList{};
    }
    auto vit = fit->second.find(value);
    if (vit == fit->second.end()) {
        return PostingList{};
    }
    return vit->second;
}

}  // namespace vectordb
