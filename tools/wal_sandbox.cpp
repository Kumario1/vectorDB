// Stage 1 — WAL sandbox template
// Fill in the TODOs. Goal: write one INSERT record, read it back, check fields + checksum.
//
// Record contract (v1):
//   [record_length u32] = bytes of (lsn + op + payload + checksum)  // NOT including this field
//   [lsn           u64]
//   [op_type       u32]   1=INSERT  2=UPDATE  3=DELETE  4=CHECKPOINT
//   [payload...]
//   [checksum      u32]   byte sum of lsn + op + payload  (NOT the checksum field itself)
//
// INSERT payload:
//   [id u64][dimensions u32][floats × dimensions]

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <cassert>

namespace {

constexpr std::uint32_t kOpInsert = 1;
// constexpr std::uint32_t kOpUpdate = 2;
// constexpr std::uint32_t kOpDelete = 3;
// constexpr std::uint32_t kOpCheckpoint = 4;

void add_bytes(std::uint32_t& checksum, const void* ptr, std::size_t n) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(ptr);
    for (std::size_t i = 0; i < n; ++i) {
        checksum += p[i];
    }
}

}  // namespace

int main() {
    std::filesystem::create_directories("data");
    const char* path = "data/wal_sandbox.wal";

    // ----- sample INSERT -----
    const std::uint64_t lsn = 1;
    const std::uint32_t op = kOpInsert;
    const std::uint64_t id = 42;
    const std::uint32_t dimensions = 2;
    const float floats[2] = {1.0f, 2.0f};

    // payload size = 8 + 4 + 4*dimensions
    // body size (record_length) = 8(lsn) + 4(op) + payload + 4(checksum)
    const std::uint32_t payload_bytes =
        8 + 4 + static_cast<std::uint32_t>(sizeof(float) * dimensions);
    const std::uint32_t record_length =
        8 + 4 + payload_bytes + 4;  // lsn + op + payload + checksum

    // ========== WRITE ==========
    FILE* out = std::fopen(path, "wb");
    if (!out) {
        std::cerr << "fopen write failed\n";
        return 1;
    }

    std::uint32_t checksum = 0;

    // TODO 1: fwrite record_length  (4 bytes, once)  — do NOT add_bytes this into checksum
    std::fwrite(&record_length, 4, 1, out);

    // TODO 2: fwrite lsn; add_bytes(checksum, &lsn, 8);
    std::fwrite(&lsn, 8, 1, out);
    add_bytes(checksum, &lsn, 8);
    // TODO 3: fwrite op;  add_bytes(checksum, &op, 4);
    std::fwrite(&op, 4, 1, out);
    add_bytes(checksum, &op, 4);
    // TODO 4: fwrite INSERT payload in order: id, dimensions, floats
    //         add_bytes after each (or once over the whole payload)
    std::fwrite(&id, 8, 1, out);
    add_bytes(checksum, &id, 8);
    std::fwrite(&dimensions, 4, 1, out);
    add_bytes(checksum, &dimensions, 4);

    std::fwrite(floats, sizeof(float), dimensions, out);
    add_bytes(checksum, floats, sizeof(float) * dimensions);
    // TODO 5: fwrite &checksum (4 bytes)  — do NOT add_bytes the checksum into itself
    std::fwrite(&checksum, 4, 1, out);


    //==========LSN 2==========
    const std::uint64_t lsn2 = 2;
    const std::uint32_t op2 = kOpInsert;
    const std::uint64_t id2 = 99;
    const std::uint32_t dimensions2 = 2;
    const float floats2[2] = {3.0f, 4.0f};
    const std::uint32_t payload_bytes2 =
        8 + 4 + static_cast<std::uint32_t>(sizeof(float) * dimensions2);
    const std::uint32_t record_length2 = 8 + 4 + payload_bytes2 + 4;

    checksum = 0;

    std::fwrite(&record_length2, 4, 1, out);

    std::fwrite(&lsn2, 8, 1, out);
    add_bytes(checksum, &lsn2, 8);

    std::fwrite(&op2, 4, 1, out);
    add_bytes(checksum, &op2, 4);

    std::fwrite(&id2, 8, 1, out);
    add_bytes(checksum, &id2, 8);

    std::fwrite(&dimensions2, 4, 1, out);
    add_bytes(checksum, &dimensions2, 4);

    std::fwrite(floats2, sizeof(float), dimensions2, out);
    add_bytes(checksum, floats2, sizeof(float) * dimensions2);

    std::fwrite(&checksum, 4, 1, out);


    std::fclose(out);

    // ========== READ ==========
    FILE* in = std::fopen(path, "rb");
    if (!in) {
        std::cerr << "fopen read failed\n";
        return 1;
    }

    while (true) {

        std::uint32_t got_length = 0;
    // TODO 6: fread got_length; if got != 1 → fail
    //         if got_length != record_length → fail (for this single-record file)
    size_t n = std::fread(&got_length, 4, 1, in);
    if (n != 1) {
        break;  // EOF
    }
    std::uint32_t check = 0;

    std::uint64_t got_lsn = 0;
    // TODO 7: fread got_lsn; add_bytes(check, &got_lsn, 8);
    std::fread(&got_lsn, 8, 1, in);
    add_bytes(check, &got_lsn, 8);

    std::uint32_t got_op = 0;
    // TODO 8: fread got_op; add_bytes(check, &got_op, 4);
    std::fread(&got_op, 4, 1, in);
    add_bytes(check, &got_op, 4);

    std::uint64_t got_id = 0;
    std::uint32_t got_dims = 0;
    float got_floats[2] = {};
    // TODO 9: fread id, dims, floats; add_bytes each into check
    std::fread(&got_id, 8, 1, in);
    add_bytes(check, &got_id, 8);
    std::fread(&got_dims, 4, 1, in);
    add_bytes(check, &got_dims, 4);
    std::fread(got_floats, sizeof(float), got_dims, in);
    add_bytes(check, got_floats, sizeof(float) * got_dims);

    std::uint32_t got_checksum = 0;
    // TODO 10: fread got_checksum; do NOT add_bytes it
    //          if (got_checksum != check) → fail
    std::fread(&got_checksum, 4, 1, in);
    if (got_checksum != check) {
        std::cerr << "got_checksum != check\n";
        return 1;
    }

    std::cout << "got_length: " << got_length << std::endl;
    std::cout << "got_lsn: " << got_lsn << std::endl;
    std::cout << "got_op: " << got_op << std::endl;
    std::cout << "got_id: " << got_id << std::endl;
    std::cout << "got_dims: " << got_dims << std::endl;
    std::cout << "got_floats: " << got_floats[0] << ", " << got_floats[1] << std::endl;
    std::cout << "got_checksum: " << got_checksum << std::endl;

    // Per-record checks: both records share the same body size here.
    // Checksum: compare to recomputed `check`, not the write-side `checksum` variable
    // (that variable only holds the last record's sum after the writes).
    assert(got_length == record_length);
    assert(got_length == record_length2);
    assert(got_op == kOpInsert);
    assert(got_checksum == check);

    if (got_lsn == 1) {
        assert(got_id == id);
        assert(got_dims == dimensions);
        assert(got_floats[0] == floats[0]);
        assert(got_floats[1] == floats[1]);
    } else if (got_lsn == 2) {
        assert(got_id == id2);
        assert(got_dims == dimensions2);
        assert(got_floats[0] == floats2[0]);
        assert(got_floats[1] == floats2[1]);
    } else {
        assert(false && "unexpected LSN");
    }

    }

    std::fclose(in);
    std::cout << "wal_sandbox: 2 records ok\n";
    return 0;
}
