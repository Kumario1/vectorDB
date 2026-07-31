#include "vectordb/flat_vector_store.hpp"
#include "vectordb/vector_store.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

template <typename Store>
void fill_store(Store& store, std::size_t dims, std::size_t count) {
    std::vector<float> values(dims, 1.0f);
    for (std::size_t i = 0; i < count; ++i) {
        if (!store.append(static_cast<std::uint64_t>(i), values)) {
            std::cerr << "append failed at " << i << '\n';
            std::abort();
        }
    }
}

// Four accumulators so floating-point adds can pipeline instead of forming
// one long serial dependency chain (which would hide memory-layout effects).
double scan_a(const vectordb::VectorStore& store) {
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    for (std::size_t i = 0; i < store.size(); ++i) {
        const auto& values = store.at(i).values;
        std::size_t j = 0;
        for (; j + 4 <= values.size(); j += 4) {
            s0 += values[j];
            s1 += values[j + 1];
            s2 += values[j + 2];
            s3 += values[j + 3];
        }
        for (; j < values.size(); ++j) {
            s0 += values[j];
        }
    }
    return s0 + s1 + s2 + s3;
}

double scan_b(const vectordb::FlatVectorStore& store) {
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    for (std::size_t i = 0; i < store.size(); ++i) {
        const auto values = store.values_at(i);
        std::size_t j = 0;
        for (; j + 4 <= values.size(); j += 4) {
            s0 += values[j];
            s1 += values[j + 1];
            s2 += values[j + 2];
            s3 += values[j + 3];
        }
        for (; j < values.size(); ++j) {
            s0 += values[j];
        }
    }
    return s0 + s1 + s2 + s3;
}

template <typename Fn>
double median_seconds(Fn&& fn, int runs = 5) {
    fn();  // warm-up; discard timing

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

void run_case(std::size_t dims, std::size_t count) {
    vectordb::VectorStore store_a(dims);
    vectordb::FlatVectorStore store_b(dims);
    fill_store(store_a, dims, count);
    fill_store(store_b, dims, count);

    double sum_a = 0.0;
    double sum_b = 0.0;
    const double time_a = median_seconds([&] { sum_a = scan_a(store_a); });
    const double time_b = median_seconds([&] { sum_b = scan_b(store_b); });
    const double ratio = time_b > 0.0 ? time_a / time_b : 0.0;

    std::cout << dims << ", " << count << ", " << time_a << ", " << time_b << ", "
              << ratio << ", " << sum_a << ", " << sum_b << '\n';
}

}  // namespace

int main() {
    std::cout << "dims, n, a_s, b_s, a_over_b, sum_a, sum_b\n";
    for (std::size_t dims : {4ull, 128ull}) {
        for (std::size_t count : {10'000ull, 100'000ull, 1'000'000ull}) {
            run_case(dims, count);
        }
    }
    return 0;
}
