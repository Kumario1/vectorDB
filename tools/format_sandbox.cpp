#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <filesystem>

int main() {
    const char* path = "data/format_sandbox.bin";
    const char magic[8] = {'V','E','C','D','B','0','0','1'};
    const std::uint32_t version = 1;
    const std::uint32_t dimensions = 2;
    const std::uint64_t record_count = 2;
    const std::uint64_t active_count = 2;
    const std::uint32_t metric = 0;
    // sample data
    std::uint64_t ids[2] = {101, 55};
    std::uint8_t deleted[2] = {0, 0};
    float floats[4] = {0.1f, 0.2f, 1.0f, 0.0f};

    std::uint32_t checksum = 0;

    auto add_bytes = [&](const void* ptr, std::size_t n) {
        for (std::size_t i = 0; i < n; i++) {
            checksum += reinterpret_cast<const std::uint8_t*>(ptr)[i];
        }
    };

    FILE* in = fopen(path, "wb");
    fwrite(magic, 1, 8, in);
    add_bytes(&magic, 8);
    fwrite(&version, 4, 1, in);
    add_bytes(&version, 4);
    fwrite(&dimensions, 4, 1, in);
    add_bytes(&dimensions, 4);
    fwrite(&record_count, 8, 1, in);
    add_bytes(&record_count, 8);
    fwrite(&active_count, 8, 1, in);
    add_bytes(&active_count, 8);
    fwrite(&metric, 4, 1, in);
    add_bytes(&metric, 4);
    fwrite(ids, 8, 2, in);
    add_bytes(&ids, 16);
    fwrite(deleted, 1, 2, in);
    add_bytes(&deleted, 2);
    fwrite(floats, 4, 4, in);
    add_bytes(floats, 16);
    fwrite(&checksum, 4, 1, in);
    fclose(in);
    
    // --- WRITE ---
    // 1) fopen path for binary write ("wb") — create data/ if needed
    // 2) fwrite magic, 1 byte, 8 times (or count=8, size=1)
    // 3) fwrite &version, 4 bytes, once
    // 4) fclose

    checksum = 0;

    FILE* out = fopen(path, "rb");

    char got_magic[8];
    fread(got_magic, 1, 8, out);
    add_bytes(&got_magic, 8);

    std::uint32_t got_version;
    fread(&got_version, 4, 1, out);
    add_bytes(&got_version, 4);

    std::uint32_t got_dimensions;
    fread(&got_dimensions, 4, 1, out);
    add_bytes(&got_dimensions, 4);

    std::uint64_t got_record_count;
    fread(&got_record_count, 8, 1, out);
    add_bytes(&got_record_count, 8);

    std::uint64_t got_active_count;
    fread(&got_active_count, 8, 1, out);
    add_bytes(&got_active_count, 8);

    std::uint32_t got_metric;
    fread(&got_metric, 4, 1, out);
    add_bytes(&got_metric, 4);

    std::uint64_t got_ids[2];
    fread(got_ids, 8, 2, out);
    add_bytes(&got_ids, 16);

    std::uint8_t got_deleted[2];
    fread(got_deleted, 1, 2, out);
    add_bytes(&got_deleted, 2);

    float got_floats[4];
    fread(got_floats, 4, 4, out);
    add_bytes(&got_floats, 16);

    std::uint32_t got_checksum;
    fread(&got_checksum, 4, 1, out);
    if (checksum != got_checksum) {
        std::cerr << "Checksum mismatch: " << got_checksum << std::endl;
    }
    fclose(out);
    if (memcmp(magic, got_magic, 8) != 0) {
        std::cerr << "Magic mismatch: " << got_magic << std::endl;
    }
    if (got_version != version) {
        std::cerr << "Version mismatch: " << got_version << std::endl;
    }
    if (got_dimensions != dimensions) {
        std::cerr << "Dimensions mismatch: " << got_dimensions << std::endl;
    }
    if (got_record_count != record_count) {
        std::cerr << "Record count mismatch: " << got_record_count << std::endl;
    }
    if (got_active_count != active_count) {
        std::cerr << "Active count mismatch: " << got_active_count << std::endl;
    }
    if (got_metric != metric) {
        std::cerr << "Metric mismatch: " << got_metric << std::endl;
    }
    if (memcmp(ids, got_ids, 16) != 0) {
        std::cerr << "IDs mismatch: " << got_ids << std::endl;
    }
    if (memcmp(deleted, got_deleted, 2) != 0) {
        std::cerr << "Deleted mismatch: " << got_deleted << std::endl;
    }
    if (memcmp(floats, got_floats, 16) != 0) {
        std::cerr << "Floats mismatch: " << got_floats << std::endl;
    }
    // --- READ ---
    // 1) fopen "rb"
    // 2) read 8 bytes into char got_magic[8]
    // 3) read 4 bytes into uint32_t got_version
    // 4) fclose
    // 5) compare magic with memcmp; print both versions

    return 0;
}