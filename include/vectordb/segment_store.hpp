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
    explicit SegmentStore(std::string dir,
                          std::size_t dimensions,
                          Metric metric = Metric::cosine,
                          std::size_t flush_threshold_rows = 1024);

    Status open();
    Status put(std::uint64_t id, const std::vector<float>& values);
    Status remove(std::uint64_t id);
    Status flush();  // flush_memtable -> new segment name -> Manifest::add -> replace
    Status compact();

    std::optional<std::vector<float>> get(std::uint64_t id) const;
    std::vector<SearchResult> search(const std::vector<float>& query, std::size_t k) const;

private:
    std::string dir_;
    Metric metric_;
    Memtable memtable_;
    Manifest manifest_;
};

}  // namespace vectordb
