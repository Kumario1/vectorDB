#pragma once

#include "vectordb/flat_vector_store.hpp"
#include "vectordb/id_index.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <memory>

namespace vectordb {


enum class Metric { cosine, dot_product, euclidean };

enum class Status {
    ok,
    duplicate_id,
    not_found,
    dimension_mismatch,
    invalid_argument
};


struct SearchResult {
    std::uint64_t id;
    float score;
};

struct WalRecord;

class WalWriter;


class VectorDB {
public:
    explicit VectorDB(std::size_t dimensions, Metric metric = Metric::cosine);

    Status save(const std::string& path) const;
    Status load(const std::string& path);


    Status open(const std::string& snapshot_path, const std::string& wal_path);
    Status checkpoint(const std::string& snapshot_path);

    VectorDB(VectorDB&&) noexcept;
    VectorDB& operator=(VectorDB&&) noexcept;
    VectorDB(const VectorDB&) = delete;
    VectorDB& operator=(const VectorDB&) = delete;
    ~VectorDB();

    Status replay_wal(const std::string& wal_path, uint64_t& out_max_lsn);

    Status insert(std::uint64_t id, std::span<const float> values);
    Status update(std::uint64_t id, std::span<const float> values);
    Status remove(std::uint64_t id);
    Status enable_wal(const std::string& path);

    std::optional<std::span<const float>> get(std::uint64_t id) const;
    std::vector<SearchResult> search(std::span<const float> query, std::size_t k) const;
    float score_pair(std::span<const float> query, std::span<const float> candidate) const;
    std::size_t physical_size() const noexcept;
    std::uint64_t id_at(std::size_t index) const noexcept;
    bool is_deleted_at(std::size_t index) const noexcept;
    std::span<const float> values_at(std::size_t index) const noexcept;

    std::size_t dimensions() const noexcept;
    Metric metric() const noexcept;
    std::size_t size() const noexcept;  // active (non-deleted) count

private:
    Metric metric_;
    FlatVectorStore store_;
    IdIndex index_;
    std::size_t active_count_ = 0;  // optional: track live rows without scanning
    std::unique_ptr<WalWriter> wal_; // pointer to the WAL writer

    std::string wal_path_;
    Status apply_wal_record(const WalRecord& rec);
};

}  // namespace vectordb