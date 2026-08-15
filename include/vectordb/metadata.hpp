#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace vectordb {

using MetadataValue = std::variant<std::int64_t, double, bool, std::string>;
using Metadata = std::unordered_map<std::string, MetadataValue>;

// Missing key or wrong alternative → nullopt.
template <typename T>
std::optional<T> get_field(const Metadata& meta, std::string_view key) {
    auto it = meta.find(std::string(key));
    if (it == meta.end()) {
        return std::nullopt;
    }
    if (const T* p = std::get_if<T>(&it->second)) {
        return *p;
    }
    return std::nullopt;
}

}  // namespace vectordb
