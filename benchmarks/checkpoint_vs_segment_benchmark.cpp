#include "vectordb/database.hpp"
#include "vectordb/segment.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

template <typename Fn>
double median_seconds(Fn&& fn, int runs = 5) {
    fn();  // warm-up

    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(runs));
    for (int i = 0; i < runs; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        const auto end = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(end - start).count());
    }
    std::sort(times.begin(), times.end());
    return times[static_cast<std::size_t>(runs) / 2];
}

vectordb::VectorDB make_db(std::size_t dims, std::size_t n) {
    vectordb::VectorDB db(dims, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    std::vector<float> values(dims, 1.0f);
    for (std::size_t i = 0; i < n; ++i) {
        if (db.insert(static_cast<std::uint64_t>(i), values) != vectordb::Status::ok) {
            std::cerr << "insert failed at " << i << '\n';
            std::abort();
        }
    }
    return db;
}

void write_segment_batch(const fs::path& path,
                         std::size_t dims,
                         std::size_t batch,
                         std::uint64_t id_base) {
    vectordb::SegmentWriter writer(path.string());
    if (writer.open(dims, vectordb::Metric::cosine) != vectordb::Status::ok) {
        std::cerr << "segment open failed\n";
        std::abort();
    }
    std::vector<float> values(dims, 1.0f);
    for (std::size_t i = 0; i < batch; ++i) {
        vectordb::SegmentRow row;
        row.id = id_base + static_cast<std::uint64_t>(i);
        row.is_deleted = false;
        row.values = values;
        if (writer.append(row) != vectordb::Status::ok) {
            std::cerr << "segment append failed\n";
            std::abort();
        }
    }
    if (writer.finish() != vectordb::Status::ok) {
        std::cerr << "segment finish failed\n";
        std::abort();
    }
}

void run_case(const fs::path& dir,
              std::size_t dims,
              std::size_t n,
              std::size_t batch) {
    auto db = make_db(dims, n);
    const auto snap = dir / ("checkpoint_" + std::to_string(n) + ".vdb");
    const auto seg = dir / ("segment_batch_" + std::to_string(batch) + ".seg");

    const double t_checkpoint = median_seconds([&] {
        if (db.save(snap.string()) != vectordb::Status::ok) {
            std::cerr << "checkpoint save failed\n";
            std::abort();
        }
    });

    const double t_segment = median_seconds([&] {
        write_segment_batch(seg, dims, batch, /*id_base=*/n);
    });

    const auto checkpoint_bytes = fs::file_size(snap);
    const auto segment_bytes = fs::file_size(seg);
    const double time_ratio =
        t_segment > 0.0 ? t_checkpoint / t_segment : 0.0;
    const double bytes_ratio =
        segment_bytes > 0
            ? static_cast<double>(checkpoint_bytes) /
                  static_cast<double>(segment_bytes)
            : 0.0;

    std::cout << dims << ", " << n << ", " << batch << ", " << t_checkpoint
              << ", " << t_segment << ", " << time_ratio << ", "
              << checkpoint_bytes << ", " << segment_bytes << ", " << bytes_ratio
              << '\n';
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "vectordb_ckpt_vs_seg";
    fs::create_directories(dir);

    std::cout << "dims, n, batch, checkpoint_s, segment_s, time_ckpt_over_seg, "
                 "checkpoint_bytes, segment_bytes, bytes_ckpt_over_seg\n";

    // Fixed embedding width; vary DB size and flush batch.
    constexpr std::size_t kDims = 128;
    for (std::size_t n : {10'000ull, 100'000ull}) {
        for (std::size_t batch : {1'000ull, 10'000ull}) {
            if (batch > n) {
                continue;
            }
            run_case(dir, kDims, n, batch);
        }
    }

    fs::remove_all(dir);
    return 0;
}
