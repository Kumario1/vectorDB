#include "vectordb/memtable.hpp"

#include <algorithm>
namespace vectordb {

    Memtable::Memtable(std::size_t dimensions, std::size_t flush_threshold_rows)
        : dimensions_(dimensions), flush_threshold_rows_(flush_threshold_rows) {}

    Status Memtable::put(std::uint64_t id, const std::vector<float>& values) {
        if (values.size() != dimensions_) {
            return Status::dimension_mismatch;
        }
        entries_[id] = { false, values };
        return Status::ok;
    }

    Status Memtable::remove(std::uint64_t id) {
        auto it = entries_.find(id);
        if (it == entries_.end()) {
            return Status::not_found;
        }
        it->second.is_deleted = true;
        it->second.values.clear();
        return Status::ok;
    }

    std::optional<std::span<const float>> Memtable::get(std::uint64_t id) const {
        auto it = entries_.find(id);
        if (it == entries_.end() || it->second.is_deleted) {
            return std::nullopt;
        }
        return std::span<const float>(it->second.values);
    }

    std::size_t Memtable::size() const noexcept {
        return entries_.size();
    }

    std::size_t Memtable::live_count() const noexcept {
        return std::count_if(entries_.begin(), entries_.end(), [](const auto& entry) { return !entry.second.is_deleted; });
    }

    bool Memtable::needs_flush() const noexcept {
        return size() >= flush_threshold_rows_;
    }

    Memtable::const_iterator Memtable::begin() const noexcept {
        return entries_.begin();
    }

    Memtable::const_iterator Memtable::end() const noexcept {
        return entries_.end();
    }

} // namespace vectordb