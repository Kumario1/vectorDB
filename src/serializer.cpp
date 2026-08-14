#include "vectordb/serializer.hpp"

#include <cstdio>
#include <span>
#include <cstring>   // memcmp
#include <vector>

namespace vectordb {

std::uint32_t metric_to_u32(Metric m) {
    // switch (m):
    //   cosine       → return 0
    //   dot_product  → return 1
    //   euclidean    → return 2
    switch (m) {
        case Metric::cosine:
            return 0;
        case Metric::dot_product:
            return 1;
        case Metric::euclidean:
            return 2;
    }
    return 0;
}

Metric metric_from_u32(std::uint32_t v) {
    // switch (v):
    //   0 → cosine
    //   1 → dot_product
    //   2 → euclidean
    //   default → cosine  (or whatever you chose)
    switch (v) {
        case 0:
            return Metric::cosine;
        case 1:
            return Metric::dot_product;
        case 2:
            return Metric::euclidean;
    }
    return Metric::cosine;
}

void add_bytes(std::uint32_t& checksum, const void* ptr, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        checksum += reinterpret_cast<const std::uint8_t*>(ptr)[i];
    }
}

void write_header(FILE* file, const VectorDB& db, std::uint32_t& checksum) {
    fwrite(&kMagic, 1, 8, file);
    add_bytes(checksum, &kMagic, 8);
    fwrite(&kFormatVersion, 4, 1, file);
    add_bytes(checksum, &kFormatVersion, 4);
    
    uint32_t dimensions =static_cast<uint32_t>(db.dimensions());
    uint64_t active = db.size();
    uint32_t metric_u32 = metric_to_u32(db.metric());
    uint64_t record_count = db.physical_size();

    fwrite(&dimensions, 4, 1, file);
    add_bytes(checksum, &dimensions, 4);

    fwrite(&record_count, 8, 1, file);
    add_bytes(checksum, &record_count, 8);

    fwrite(&active, 8, 1, file);
    add_bytes(checksum, &active, 8);

    fwrite(&metric_u32, 4, 1, file);
    add_bytes(checksum, &metric_u32, 4);
}

void write_payload(FILE* file, const VectorDB& db, std::uint32_t& checksum) {
    const std::size_t n = db.physical_size();

    // Section 1: all ids
    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t id = db.id_at(i);
        fwrite(&id, 8, 1, file);
        add_bytes(checksum, &id, 8);
    }

    // Section 2: all tombstone flags, one byte each
    for (std::size_t i = 0; i < n; ++i) {
        std::uint8_t d = db.is_deleted_at(i) ? 1 : 0;
        fwrite(&d, 1, 1, file);
        add_bytes(checksum, &d, 1);
    }

    // Section 3: all float components
    for (std::size_t i = 0; i < n; ++i) {
        std::span<const float> values = db.values_at(i);
        fwrite(values.data(), 4, values.size(), file);
        add_bytes(checksum, values.data(), values.size() * 4);
    }
}


Status save_database(const VectorDB& db, const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return Status::invalid_argument;  // open failed; io_error later if we add it
    }

    std::uint32_t checksum = 0;
    write_header(file, db, checksum);
    write_payload(file, db, checksum);
    // trailing checksum — do not fold these bytes into the sum
    std::fwrite(&checksum, 4, 1, file);
    std::fclose(file);
    return Status::ok;
}

Status load_database(const std::string& path, VectorDB& db) {
    // stub for now
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file){
        return Status::invalid_argument;
    }

    std::uint32_t checksum = 0;
    //read header
    char magic[8];
    size_t got = std::fread(magic, 1, 8, file);
    if (got != 8){
        std::fclose(file);
        return Status::invalid_argument;
    }
    if (std::memcmp(magic, kMagic, 8) != 0){
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, magic, 8);

    std::uint32_t version = 0;
    got = std::fread(&version, 4, 1, file);
    if (got != 1) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    if (version != kFormatVersion) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, &version, 4);

    std::uint32_t dimensions = 0;
    got = std::fread(&dimensions, 4, 1, file);
    if (got != 1 || dimensions == 0) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, &dimensions, 4);

    std::uint64_t record_count = 0;
    got = std::fread(&record_count, 8, 1, file);
    if (got != 1) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, &record_count, 8);

    std::uint64_t active = 0;
    got = std::fread(&active, 8, 1, file);
    if (got != 1 || active > record_count) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, &active, 8);

    std::uint32_t metric = 0;
    got = std::fread(&metric, 4, 1, file);
    if (got != 1 || metric > 2) {
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, &metric, 4);

    //read payload

    //Section 1: all ids
    std::vector<std::uint64_t> ids(record_count);
    got = std::fread(ids.data(), 8, record_count, file);
    if (got != record_count){
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, ids.data(), record_count * 8);
    
    //section 2: all tombstone flags
    std::vector<std::uint8_t> deleted(record_count);
    got = std::fread(deleted.data(), 1, record_count, file);
    if (got != record_count){
        std::fclose(file);
        return Status::invalid_argument;
    }
    add_bytes(checksum, deleted.data(), record_count);

    //section 3: all float components
    std::vector<float> values(record_count * dimensions);
    got = std::fread(values.data(), 4, record_count * dimensions, file);
    if (got != record_count * dimensions){
        std::fclose(file);
        return Status::invalid_argument;
    }

    add_bytes(checksum, values.data(), record_count * dimensions * 4);
    
    //read trailing checksum
    std::uint32_t trailing_checksum = 0;
    got = std::fread(&trailing_checksum, 4, 1, file);
    if (got != 1 ){
        std::fclose(file);
        return Status::invalid_argument;
    }
    if (trailing_checksum != checksum){
        std::fclose(file);
        return Status::invalid_argument;
    }

    //build database
    db = VectorDB(dimensions, metric_from_u32(metric), StorageMode::legacy);

    for (std::size_t i = 0; i < record_count; ++i){
        std::span<const float> vals(values.data() + i * dimensions, dimensions);
        db.insert(ids[i], vals);
        if (deleted[i]){
            db.remove(ids[i]);
        }
    }

    std::fclose(file);
    return Status::ok;
    



}

}  // namespace vectordb