#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace vectordb {

// Reference implementation: wraps std::unordered_map.
// Later we'll swap the guts for OpenAddressHashTable without changing this API.
class IdIndex {
public:
    // Returns false if id already exists.
    bool insert(std::uint64_t id, std::size_t position);

    // Empty if id is missing.
    std::optional<std::size_t> find(std::uint64_t id) const;

    // Returns false if id was not present.
    bool erase(std::uint64_t id);

    std::size_t size() const noexcept;

    // live entries / bucket_count  (useful once we go custom; for now wrap map)
    double load_factor() const noexcept;

private:
    std::unordered_map<std::uint64_t, std::size_t> map_;
};

}  // namespace vectordb