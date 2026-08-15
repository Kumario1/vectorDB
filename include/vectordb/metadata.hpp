#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

namespace vectordb {

// Curriculum types for M8 — wire these into VectorDB in #17.
using MetadataValue = std::variant<std::int64_t, double, bool, std::string>;
using Metadata = std::unordered_map<std::string, MetadataValue>;

// One equality predicate for filtered search: field = value.
struct EqualityPredicate {
    std::string field;
    MetadataValue value;
};

}  // namespace vectordb
