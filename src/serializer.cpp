#include "vectordb/serializer.hpp"

#include <cstdio>
#include <span>

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


Status save_database(const VectorDB& /*db*/, const std::string& /*path*/) {
    // stub for now — real write comes in 5.2+
    return Status::invalid_argument;
}

Status load_database(const std::string& /*path*/, VectorDB& /*out*/) {
    // stub for now
    return Status::invalid_argument;
}

}  // namespace vectordb