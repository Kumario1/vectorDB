#pragma once

#include "vectordb/database.hpp"

#include <cstdint>
#include <string>

namespace vectordb {

inline constexpr char kMagic[8] = {'V','E','C','D','B','0','0','1'};
inline constexpr std::uint32_t kFormatVersion = 1;

// Metric on disk: 0=cosine, 1=dot_product, 2=euclidean
std::uint32_t metric_to_u32(Metric m);
Metric metric_from_u32(std::uint32_t v);  // pick a behavior for bad values (e.g. default cosine)

// Returns Status::ok or a failure you’ll define (we can reuse invalid_argument for now,
// or add Status::io_error later)
Status save_database(const VectorDB& db, const std::string& path);
Status load_database(const std::string& path, VectorDB& out); // or return optional/unique — we’ll decide in 5.2

}  // namespace vectordb