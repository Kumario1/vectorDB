#include "vectordb/memtable.hpp"
#include "vectordb/segment.hpp"

#include <algorithm>
#include <span>
#include <string>
namespace vectordb {

    Memtable::Memtable(std::size_t dimensions, std::size_t flush_threshold_rows)
        : dimensions_(dimensions), flush_threshold_rows_(flush_threshold_rows) {}

    Status flush_memtable(Memtable& mt, const std::string& path, Metric metric) {
        //create writer
        SegmentWriter writer(path);

        //opne writer with dimensions and metric
        Status st = writer.open(mt.dimensions(), metric);
        if (st != Status::ok) {
            return st;
        }

        //iteratre through entries and append to writer
        for (const auto& [id, entry] : mt) {
            SegmentRow row;
            row.id = id;
            row.is_deleted = entry.is_deleted;
            row.values = entry.is_deleted ? std::vector<float>() : entry.values;
            //append row to writer
            st = writer.append(row);
            if (st != Status::ok) {
                return st;
            }
        }

        //finish writing
        st = writer.finish();
        if (st != Status::ok) {
            return st;
        }
        //clear memtable
        mt.clear();
        //return ok
        return Status::ok;
    }


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

    void Memtable::tombstone(std::uint64_t id) {
        entries_[id] = {true, {}};
    }

    void Memtable::clear() {
        entries_.clear();
    }

    std::optional<std::span<const float>> Memtable::get(std::uint64_t id) const {
        auto it = entries_.find(id);
        if (it == entries_.end() || it->second.is_deleted) {
            return std::nullopt;
        }
        return std::span<const float>(it->second.values);
    }

    std::optional<MemtableEntry> Memtable::find(std::uint64_t id) const {
        auto it = entries_.find(id);
        if (it == entries_.end()) {
            return std::nullopt;
        }
        return it->second; //return the entry could be live or tombstone
    }

    std::size_t Memtable::size() const noexcept {
        return entries_.size();
    }

    std::size_t Memtable::live_count() const noexcept {
        return std::count_if(entries_.begin(), entries_.end(), [](const auto& entry) { return !entry.second.is_deleted; });
    }

    std::size_t Memtable::dimensions() const noexcept {
        return dimensions_;
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