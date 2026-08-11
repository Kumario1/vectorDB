#include "vectordb/segment.hpp"
#include "vectordb/serializer.hpp"

#include <cstring>

namespace vectordb {

// ----------------- helpers -----------------
namespace {

void add_bytes(std::uint32_t& checksum, const void* ptr, std::size_t n) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(ptr);
    for (std::size_t i = 0; i < n; ++i) {
        checksum += p[i];
    }
}

}  // namespace


// ----------------- SegmentWriter -----------------
SegmentWriter::SegmentWriter(std::string path) : path_(std::move(path)) {}

SegmentWriter::~SegmentWriter() {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

Status SegmentWriter::open(std::size_t dimensions, Metric metric) {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
    file_ = std::fopen(path_.c_str(), "wb");
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }
    rows_.clear();
    dimensions_ = dimensions;
    metric_ = metric;
    return Status::ok;
}

Status SegmentWriter::append(const SegmentRow& row) {
    if (row.is_deleted) {
        //its okay for values to be empty when deleted
        if (!row.values.empty()) {
            return Status::invalid_argument;
        }
    } else {
        //validate dimensions
        if (row.values.size() != dimensions_) {
            return Status::invalid_argument;
        }
    }
    rows_.push_back(row);
    return Status::ok;
}

Status SegmentWriter::finish() {
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }
    //write header
    std::uint32_t checksum = 0;
    fwrite(kSegmentMagic, 1, 8, file_);
    add_bytes(checksum, kSegmentMagic, 8);
    fwrite(&kSegmentVersion, 4, 1, file_);
    add_bytes(checksum, &kSegmentVersion, 4);
    //write dimensions as u32
    std::uint32_t dimensions_u32 = static_cast<std::uint32_t>(dimensions_);
    fwrite(&dimensions_u32, 4, 1, file_);
    add_bytes(checksum, &dimensions_u32, 4);
    //write record count 
    std::uint64_t record_count = rows_.size();
    fwrite(&record_count, 8, 1, file_);
    add_bytes(checksum, &record_count, 8);
    //write metric as u32
    std::uint32_t metric_u32 = metric_to_u32(metric_);
    fwrite(&metric_u32, 4, 1, file_);
    add_bytes(checksum, &metric_u32, 4);

    ///write payload using SoA

    //first write ids
    for (const auto& row: rows_) {
        fwrite(&row.id, 8, 1, file_);
        add_bytes(checksum, &row.id, 8);
    }
    //then write deleted
    for (const auto& row: rows_) {
        //write deleted as u8 0 or 1
        std::uint8_t deleted = row.is_deleted ? 1 : 0;
        fwrite(&deleted, 1, 1, file_);
        add_bytes(checksum, &deleted, 1);
    }
    //then write values
    for (const auto& row: rows_) {
        //if deleted, write 0s
        if (row.is_deleted) {
            std::vector<float> zeros(dimensions_, 0.0f);
            fwrite(zeros.data(), sizeof(float), dimensions_, file_);
            add_bytes(checksum, zeros.data(), dimensions_ * sizeof(float));
        } else {
            fwrite(row.values.data(), sizeof(float), row.values.size(), file_);
            add_bytes(checksum, row.values.data(), row.values.size() * sizeof(float));
        }
    }

    //write checksum
    fwrite(&checksum, 4, 1, file_);

    //close
    std::fclose(file_);
    file_ = nullptr;
    return Status::ok;
}

// ----------------- SegmentReader -----------------

SegmentReader::SegmentReader(std::string path) : path_(std::move(path)) {}

SegmentReader::~SegmentReader() {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

Status SegmentReader::open() {
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

Status SegmentReader::read_all(std::vector<SegmentRow>& rows) {
    if (file_ == nullptr) {
        return Status::invalid_argument;
    }

    //checksum
    std::uint32_t checksum = 0;
    
    //read magic
    char magic[8];
    if (fread(magic, 1, 8, file_) != 8) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, magic, 8);
    //verify magic
    if (memcmp(magic, kSegmentMagic, 8) != 0) {
        return Status::invalid_argument;
    }    

    //read version
    std::uint32_t version;
    if (fread(&version, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &version, 4);
    //verify version
    if (version != kSegmentVersion) {
        return Status::invalid_argument;
    }

    //read dimension
    std::uint32_t dimension;
    if (fread(&dimension, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &dimension, 4);

    //read record count
    std::uint64_t record_count;
    if (fread(&record_count, 8, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &record_count, 8);

    //read metric
    std::uint32_t metric;
    if (fread(&metric, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, &metric, 4);

    //read payload in SoA format
    
    //first read all ids
    std::vector<std::uint64_t> ids(record_count);
    if (fread(ids.data(), 8, record_count, file_) != record_count) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, ids.data(), record_count * 8);

    //then read all deleted
    std::vector<std::uint8_t> deleted(record_count);
    if (fread(deleted.data(), 1, record_count, file_) != record_count) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, deleted.data(), record_count);

    //then read all values
    const std::size_t float_count =
        static_cast<std::size_t>(record_count) * static_cast<std::size_t>(dimension);
    std::vector<float> values(float_count);
    if (fread(values.data(), sizeof(float), float_count, file_) != float_count) {
        return Status::invalid_argument;
    }
    add_bytes(checksum, values.data(), float_count * sizeof(float));

    //read checksum
    std::uint32_t checksum_read;
    if (fread(&checksum_read, 4, 1, file_) != 1) {
        return Status::invalid_argument;
    }

    //validate checksum
    if (checksum_read != checksum) {
        return Status::invalid_argument;
    }

    //popilate rows
    rows.clear();
    rows.resize(record_count);
    for (std::size_t i = 0; i < record_count; ++i) {
        rows[i].id = ids[i];
        rows[i].is_deleted = deleted[i] == 1;
        if (!rows[i].is_deleted) {
            rows[i].values = std::vector<float>(values.begin() + i * dimension, values.begin() + (i + 1) * dimension);
        } else {
            rows[i].values.clear();
        }
    }

    //close
    std::fclose(file_);
    file_ = nullptr;

    return Status::ok;
}
}