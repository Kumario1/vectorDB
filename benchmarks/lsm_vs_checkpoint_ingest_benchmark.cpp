#include "vectordb/database.hpp"
#include "vectordb/segment_store.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::uint64_t file_bytes(const fs::path& path) {
    return fs::exists(path) ? fs::file_size(path) : 0;
}

void run_case(std::size_t dims, std::size_t n, std::size_t batch) {
    const fs::path root =
        fs::temp_directory_path() / "vectordb_lsm_vs_checkpoint_ingest";
    const fs::path old_dir = root / "old_vdb";
    const fs::path lsm_dir = root / "lsm";
    fs::remove_all(root);
    fs::create_directories(old_dir);
    fs::create_directories(lsm_dir);

    std::vector<float> values(dims, 1.0f);

    std::uint64_t old_bytes_written = 0;
    const auto old_start = std::chrono::steady_clock::now();
    {
        vectordb::VectorDB db(dims, vectordb::Metric::cosine,
                              vectordb::StorageMode::legacy);
        const auto snap = old_dir / "db.vdb";
        for (std::size_t i = 0; i < n; ++i) {
            if (db.insert(static_cast<std::uint64_t>(i), values) !=
                vectordb::Status::ok) {
                std::cerr << "old insert failed\n";
                std::abort();
            }
            if ((i + 1) % batch == 0) {
                if (db.save(snap.string()) != vectordb::Status::ok) {
                    std::cerr << "old save failed\n";
                    std::abort();
                }
                // Each checkpoint rewrites the full growing snapshot.
                old_bytes_written += file_bytes(snap);
            }
        }
    }
    const auto old_end = std::chrono::steady_clock::now();
    const double old_s =
        std::chrono::duration<double>(old_end - old_start).count();

    std::uint64_t lsm_bytes_written = 0;
    const auto lsm_start = std::chrono::steady_clock::now();
    {
        vectordb::SegmentStore store(lsm_dir.string(), dims,
                                     vectordb::Metric::cosine, batch);
        if (store.open() != vectordb::Status::ok) {
            std::cerr << "lsm open failed\n";
            std::abort();
        }
        std::size_t flush_num = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (store.put(static_cast<std::uint64_t>(i), values) !=
                vectordb::Status::ok) {
                std::cerr << "lsm put failed\n";
                std::abort();
            }
            if ((i + 1) % batch == 0) {
                ++flush_num;
                if (store.flush() != vectordb::Status::ok) {
                    std::cerr << "lsm flush failed\n";
                    std::abort();
                }
                char name[64];
                std::snprintf(name, sizeof(name), "segment-%06zu.vec",
                              flush_num);
                lsm_bytes_written += file_bytes(lsm_dir / name);
                lsm_bytes_written += file_bytes(lsm_dir / "MANIFEST");
            }
        }
    }
    const auto lsm_end = std::chrono::steady_clock::now();
    const double lsm_s =
        std::chrono::duration<double>(lsm_end - lsm_start).count();

    const double time_ratio = lsm_s > 0.0 ? old_s / lsm_s : 0.0;
    const double bytes_ratio =
        lsm_bytes_written > 0
            ? static_cast<double>(old_bytes_written) /
                  static_cast<double>(lsm_bytes_written)
            : 0.0;

    std::cout << dims << ", " << n << ", " << batch << ", " << old_s << ", "
              << lsm_s << ", " << time_ratio << ", " << old_bytes_written
              << ", " << lsm_bytes_written << ", " << bytes_ratio << '\n';

    fs::remove_all(root);
}

}  // namespace

int main() {
    std::cout << "dims, n, batch, old_checkpoint_s, lsm_flush_s, "
                 "time_old_over_lsm, old_bytes_written, lsm_bytes_written, "
                 "bytes_old_over_lsm\n";

    constexpr std::size_t kDims = 128;
    for (std::size_t n : {10'000ull, 100'000ull}) {
        for (std::size_t batch : {1'000ull, 10'000ull}) {
            if (batch > n) {
                continue;
            }
            run_case(kDims, n, batch);
        }
    }
    return 0;
}
