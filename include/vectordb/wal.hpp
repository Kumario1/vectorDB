#pragma once

#include "vectordb/database.hpp"  // Status

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace vectordb {

enum class WalOp : std::uint32_t {
    Insert = 1,
    Update = 2,
    Delete = 3,
    Checkpoint = 4,
};

// In-memory view of one WAL record (after parse / before write).
struct WalRecord {
    std::uint64_t lsn = 0;
    WalOp op = WalOp::Insert;

    // INSERT / UPDATE / DELETE
    std::uint64_t id = 0;
    std::uint32_t dimensions = 0;
    std::vector<float> values;  // size == dimensions for Insert/Update

    // CHECKPOINT only: last LSN included in the snapshot
    std::uint64_t checkpoint_lsn = 0;
};

// Append-only writer. Stamps LSN (next_lsn_) on append.
class WalWriter {
public:
    explicit WalWriter(std::string path);
    ~WalWriter();

    WalWriter(const WalWriter&) = delete;
    WalWriter& operator=(const WalWriter&) = delete;

    // Open for append ("ab"). Creates file if missing.
    Status open();

    // Writes one record. For Insert/Update, values.size() must equal dimensions.
    // Sets rec.lsn to the assigned LSN on success (also returns via record).
    Status append(WalRecord& rec);

    Status flush();

    std::uint64_t next_lsn() const noexcept { return next_lsn_; }

private:
    std::string path_;
    FILE* file_ = nullptr;
    std::uint64_t next_lsn_ = 1;
};

// Sequential reader from the start of the file.
class WalReader {
public:
    explicit WalReader(std::string path);
    ~WalReader();

    WalReader(const WalReader&) = delete;
    WalReader& operator=(const WalReader&) = delete;

    Status open();

    // have_record == false and Status::ok means clean EOF.
    Status read_next(WalRecord& out, bool& have_record);

private:
    std::string path_;
    FILE* file_ = nullptr;
};

}  // namespace vectordb
