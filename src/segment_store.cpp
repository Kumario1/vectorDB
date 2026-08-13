#include "vectordb/segment_store.hpp"
#include "vectordb/memtable.hpp"
#include "vectordb/manifest.hpp"
#include "vectordb/segment.hpp"
#include "vectordb/distance.hpp"

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace vectordb {
namespace {

struct WorseFirst {
    bool operator()(const SearchResult& a, const SearchResult& b) const {
        return a.score > b.score;  // lowest score on top
    }
};

float score_pair(Metric metric, std::span<const float> query, std::span<const float> candidate) {
    switch (metric) {
        case Metric::cosine:
            return cosine_similarity(query, candidate);
        case Metric::dot_product:
            return dot_product(query, candidate);
        case Metric::euclidean:
            return -squared_euclidean(query, candidate);
    }
    return 0.f;
}

}  // namespace

SegmentStore::SegmentStore(std::string dir, std::size_t dimensions, Metric metric, std::size_t flush_threshold_rows)
    : dir_(dir), metric_(metric), memtable_(dimensions, flush_threshold_rows) {}

Status SegmentStore::open() {
    Status st = manifest_.load(dir_);
    return st;
}

Status SegmentStore::put(std::uint64_t id, const std::vector<float>& values) {
    Status st = memtable_.put(id, values);
    return st;
}

Status SegmentStore::remove(std::uint64_t id) {
    // LSM delete: must tombstone even if the live row only exists in a segment.
    memtable_.tombstone(id);
    return Status::ok;
}

Status SegmentStore::flush() {
    // get new segment name
    std::uint64_t next_id = manifest_.segments().size() + 1;

    std::ostringstream oss;
    oss << "segment-" << std::setw(6) << std::setfill('0') << next_id << ".vec";
    std::string segment_name = oss.str();
    // e.g segment-000001.vec

    // flush memtable, this will create a new segment file that will be used to store the new data

    // so get the path to the new segment file
    std::string path = dir_ + "/" + segment_name;
    // now flush the memtable to the new segment file
    Status st = flush_memtable(memtable_, path, metric_);
    if (st != Status::ok) {
        return st;
    }

    // now we need to add this new segment to the manifest and replace the old manifest file with the new file
    manifest_.add(segment_name);
    st = manifest_.replace(dir_);
    if (st != Status::ok) {
        return st;
    }

    return Status::ok;
}

std::optional<std::vector<float>> SegmentStore::get(std::uint64_t id) const {
    // 1) memtable present?
    if (auto e = memtable_.find(id)) {
        if (e->is_deleted) {
            return std::nullopt;
        }
        return e->values;
    }

    // 2) segments from newest to oldest
    for (auto it = manifest_.segments().rbegin(); it != manifest_.segments().rend(); ++it) {
        // each it is a string of the segment name
        const std::string path = dir_ + "/" + *it;

        // now open a segment reader to read the segment file
        SegmentReader reader(path);
        if (reader.open() != Status::ok) {
            continue;
        }

        std::vector<SegmentRow> rows;
        if (reader.read_all(rows) != Status::ok) {
            continue;
        }

        // now that we have all the rows from the segment file, we need to search for the id
        for (const auto& row : rows) {
            if (row.id != id) {
                continue;
            }
            // first time we see this id in the walk -> newest segment file has it
            if (row.is_deleted) {
                return std::nullopt;
            }
            return row.values;
        }

        // if it was not found in this segment file, then we check the next segment file
    }

    // if it was not found in any of the segment files, then it is not present in the database
    return std::nullopt;
}

std::vector<SearchResult> SegmentStore::search(const std::vector<float>& query, std::size_t k) const {
    if (k == 0 || query.size() != memtable_.dimensions()) {
        return {};
    }

    std::unordered_map<std::uint64_t, std::vector<float>> live;
    std::unordered_set<std::uint64_t> seen;

    // walk memtable first (skip tombstones for candidates; mark tombstones as seen)
    for (const auto& [id, entry] : memtable_) {
        seen.insert(id);
        if (!entry.is_deleted) {
            live[id] = entry.values;
        }
    }

    // walk segments newest→oldest; skip ids already seen; skip deleted (mark seen)
    for (auto it = manifest_.segments().rbegin(); it != manifest_.segments().rend(); ++it) {
        const std::string path = dir_ + "/" + *it;

        SegmentReader reader(path);
        if (reader.open() != Status::ok) {
            continue;
        }

        std::vector<SegmentRow> rows;
        if (reader.read_all(rows) != Status::ok) {
            continue;
        }

        for (const auto& row : rows) {
            if (seen.count(row.id)) {
                continue;
            }
            seen.insert(row.id);
            if (row.is_deleted) {
                continue;
            }
            live[row.id] = row.values;
        }
    }

    // top-k heap: O(n log k), same idea as VectorDB::search
    std::priority_queue<SearchResult, std::vector<SearchResult>, WorseFirst> heap;
    for (const auto& [id, values] : live) {
        heap.push({id, score_pair(metric_, query, values)});
        if (heap.size() > k) {
            heap.pop();
        }
    }

    std::vector<SearchResult> results;
    while (!heap.empty()) {
        results.push_back(heap.top());
        heap.pop();
    }
    std::reverse(results.begin(), results.end());
    return results;
}

}  // namespace vectordb
