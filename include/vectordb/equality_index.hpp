#pragma once

#include "vectordb/metadata.hpp"
#include "vectordb/posting_list.hpp"

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vectordb {

// field → value → sorted posting list of vector ids.
// MetadataValue as the value key so int64 42 and string "42" do not collide.
class EqualityIndex {
public:
    void add(std::uint64_t id, const Metadata& meta);
    // Prefer calling with the metadata that was stored for id (needed to find lists).
    void remove(std::uint64_t id, const Metadata& meta);
    void update(std::uint64_t id, const Metadata& old_meta, const Metadata& new_meta);

    // Unknown field/value → empty PostingList.
    PostingList lookup(std::string_view field, const MetadataValue& value) const;

private:
    // Inner map keeps MetadataValue ordered/hashable by alternative + payload.
    using ValueMap = std::map<MetadataValue, PostingList>;
    std::unordered_map<std::string, ValueMap> index_;
};

}  // namespace vectordb
