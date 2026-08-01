#include "vectordb/wal.hpp"

#include <cstring>

namespace vectordb {
namespace {

void add_bytes(std::uint32_t& checksum, const void* ptr, std::size_t n) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(ptr);
    for (std::size_t i = 0; i < n; ++i) {
        checksum += p[i];
    }
}

// INSERT/UPDATE payload: id(8) + dimensions(4) + floats(4*dims)
std::uint32_t insert_payload_bytes(std::uint32_t dimensions) {
    return 8u + 4u + dimensions * static_cast<std::uint32_t>(sizeof(float));
}

}  // namespace

// ----------------- WalWriter -----------------

WalWriter::WalWriter(std::string path) : path_(std::move(path)) {}

WalWriter::~WalWriter() {
    // TODO: if file_ != nullptr, fclose and set file_ = nullptr
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

Status WalWriter::open() {
    // TODO:
    // 1) if already open, fclose first (or return ok / invalid_argument — pick one)
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
    file_ = std::fopen(path_.c_str(), "ab");
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }   
    next_lsn_ = 1;
    return Status::ok;
}

Status WalWriter::flush() {
    // TODO: if !file_ → invalid_argument
    //       fflush(file_); return ok
    // (fsync later)
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }
    if (std::fflush(file_) != 0) {
        return Status::invalid_argument;
    }
    return Status::ok;
}

Status WalWriter::append(WalRecord& rec) {
    if (!file_) {
        return Status::invalid_argument;
    }

    // v1: only Insert for now
    if (rec.op != WalOp::Insert) {
        return Status::invalid_argument;
    }
    if (rec.values.size() != rec.dimensions) {
        return Status::dimension_mismatch;
    }

    // TODO A: assign LSN
    //   rec.lsn = next_lsn_;
    rec.lsn = next_lsn_;

    // TODO B: compute sizes (same as sandbox)
    //   payload_bytes = insert_payload_bytes(rec.dimensions);
    //   record_length = 8 + 4 + payload_bytes + 4;  // lsn + op + payload + checksum
    std::uint32_t payload_bytes = insert_payload_bytes(rec.dimensions);
    std::uint32_t record_length = 8 + 4 + payload_bytes + 4;  // lsn + op + payload + checksum
    // TODO C: write record_length (u32) — do NOT add_bytes
    if (std::fwrite(&record_length, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }

    // TODO D: checksum = 0
    std::uint32_t checksum = 0;
    //   write lsn (u64); add_bytes
    if (std::fwrite(&rec.lsn, 8, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &rec.lsn, 8);
    //   write op as uint32_t (static_cast); add_bytes
    if (std::fwrite(&rec.op, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &rec.op, 4);
    //   write id (u64); add_bytes
    if (std::fwrite(&rec.id, 8, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &rec.id, 8);
    //   write dimensions (u32); add_bytes
    if (std::fwrite(&rec.dimensions, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &rec.dimensions, 4);
    //   write values.data() floats; add_bytes for dimensions * sizeof(float)
    if (std::fwrite(rec.values.data(), sizeof(float), rec.dimensions, file_) != rec.dimensions) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, rec.values.data(), sizeof(float) * rec.dimensions);
    // TODO E: write checksum (u32) — do NOT add_bytes
    if (std::fwrite(&checksum, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    // TODO F: ++next_lsn_; return Status::ok
    next_lsn_++;
    return Status::ok;
}

// ----------------- WalReader -----------------

WalReader::WalReader(std::string path) : path_(std::move(path)) {}

WalReader::~WalReader() {
    // TODO: fclose if open
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

Status WalReader::open() {
    // TODO: fopen path_ "rb"; fail → invalid_argument
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
    file_ = std::fopen(path_.c_str(), "rb");
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }
    return Status::ok;
}

Status WalReader::read_next(WalRecord& out, bool& have_record) {
    have_record = false;
    if (!file_) {
        return Status::invalid_argument;
    }

    // TODO 1: fread record_length
    //   if fread returns 0 → clean EOF: have_record=false; return ok
    //   if fread returns 1 → continue
    //   else → invalid_argument (partial read)
    std::uint32_t record_length = 0;
    std::size_t n = std::fread(&record_length, 4, 1, file_);
    if (n == 0) {
        have_record = false;
        return Status::ok;
    }
    if (n != 1) {
        return Status::invalid_argument;
    }

    // TODO 2: checksum check = 0
    std::uint32_t checksum = 0;
    //   fread lsn; add_bytes
    if (std::fread(&out.lsn, 8, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &out.lsn, 8);
    //   fread op_u32; add_bytes; out.op = static_cast<WalOp>(op_u32)
    if (std::fread(&out.op, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &out.op, 4);
    out.op = static_cast<WalOp>(out.op);

    // TODO 3: switch on out.op
    //   Insert/Update:
    //     fread id, dimensions; add_bytes
    //     out.values.resize(dimensions);
    //     fread values; add_bytes
    //   Delete:
    //     fread id; add_bytes; clear values/dims
    //   Checkpoint:
    //     fread checkpoint_lsn; add_bytes
    //   default: invalid_argument

    switch (out.op) {
        case WalOp::Insert:
        case WalOp::Update:
            if (std::fread(&out.id, 8, 1, file_) != 1) {
                return Status::invalid_argument;
            }
            add_bytes(checksum, &out.id, 8);
            if (std::fread(&out.dimensions, 4, 1, file_) != 1) {
                return Status::invalid_argument;
            }
            add_bytes(checksum, &out.dimensions, 4);
            out.values.resize(out.dimensions);
            if (std::fread(out.values.data(), sizeof(float), out.dimensions, file_) != out.dimensions) {
                return Status::invalid_argument;
            }
            add_bytes(checksum, out.values.data(), sizeof(float) * out.dimensions);
            break;

        case WalOp::Delete:
            if (std::fread(&out.id, 8, 1, file_) != 1) {
                return Status::invalid_argument;
            }
            add_bytes(checksum, &out.id, 8);
            out.values.clear();
            out.dimensions = 0;
            break;

        case WalOp::Checkpoint:
            if (std::fread(&out.checkpoint_lsn, 8, 1, file_) != 1) {
                return Status::invalid_argument;
            }
            add_bytes(checksum, &out.checkpoint_lsn, 8);
            break;

        default:
            return Status::invalid_argument;
    }

    // TODO 4: fread trailing checksum; compare to check
    //   mismatch → invalid_argument
    std::uint32_t got_checksum = 0;
    if (std::fread(&got_checksum, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    if (checksum != got_checksum) {
        return Status::invalid_argument;
    }

    // TODO 5: have_record = true; return ok
    have_record = true;
    return Status::ok;
}

}  // namespace vectordb
