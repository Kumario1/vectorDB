#pragma once

#include "vectordb/database.hpp"
#include "vectordb/memtable.hpp"
#include "vectordb/manifest.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vectordb {

class SegmentStore {
public:
    // dir may be empty: memtable-only until open(dir).
    explicit SegmentStore(std::string dir,
                          std::size_t dimensions,
                          Metric metric = Metric::cosine,
                          std::size_t flush_threshold_rows = 1024);

    Status open();                       // load MANIFEST from dir_ (dir_ must be set)
    Status open(const std::string& dir); // set dir_, then load MANIFEST
    Status put(std::uint64_t id, const std::vector<float>& values);
    Status remove(std::uint64_t id);
    Status flush();  // requires dir_; skip if memtable empty
    Status compact();  // requires dir_

    std::optional<std::vector<float>> get(std::uint64_t id) const;
    std::vector<SearchResult> search(const std::vector<float>& query, std::size_t k) const;

    bool dir_opened() const noexcept;
    bool needs_flush() const noexcept;
    std::size_t live_count() const;

private:
    Status maybe_auto_flush();
    std::uint64_t next_segment_id() const;
    std::string make_segment_name(std::uint64_t id) const;

    std::string dir_;
    Metric metric_;
    Memtable memtable_;
    Manifest manifest_;
};

}  // namespace vectordb
