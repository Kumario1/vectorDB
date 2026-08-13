#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <span>
#include <map>
#include "vectordb/database.hpp"


//--------------------------------
// insert(42, vec)  → map[42] = { live, vec }
// update(42, vec2) → map[42] = { live, vec2 }   // same id, overwrite
// remove(42)       → map[42] = { tombstone }     // id still in map
// get(42)          → nullopt if missing OR tombstone
// get(55)          → values if live
// size()           → map.size()  (includes tombstones)
// needs_flush()      → size() >= threshold_
//--------------------------------

namespace vectordb {

    struct MemtableEntry {
        bool is_deleted = false;
        std::vector<float> values;
    };

    class Memtable {
        public:
            explicit Memtable(std::size_t dimensions, std::size_t flush_threshold_rows = 1024);

            Status put(std::uint64_t id, const std::vector<float>& values);
            Status remove(std::uint64_t id);
            std::optional<std::span<const float>> get(std::uint64_t id) const;

            std::size_t size() const noexcept;
            std::size_t live_count() const noexcept;
            bool needs_flush() const noexcept;

            // for #11 we need to iterate in key order to feed SegementWrite, so it can write in the file
            using const_iterator = std::map<std::uint64_t, MemtableEntry>::const_iterator;
            const_iterator begin() const noexcept;
            const_iterator end() const noexcept;

        private:
            std::size_t dimensions_;
            std::size_t flush_threshold_rows_;
            std::map<std::uint64_t, MemtableEntry> entries_;

    };

} // namespace vectordb