#include "vectordb/segment_store.hpp"
#include "vectordb/memtable.hpp"
#include "vectordb/manifest.hpp"
#include "vectordb/segment.hpp"
#include "vectordb/distance.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <string_view>
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

std::uint64_t parse_segment_id(const std::string& name) {
    static constexpr std::string_view prefix = "segment-";
    static constexpr std::string_view suffix = ".vec";
    if (name.size() <= prefix.size() + suffix.size()) {
        return 0;
    }
    if (name.compare(0, prefix.size(), prefix) != 0) {
        return 0;
    }
    if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return 0;
    }
    const std::string num = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    try {
        return std::stoull(num);
    } catch (...) {
        return 0;
    }
}

}  // namespace

SegmentStore::SegmentStore(std::string dir, std::size_t dimensions, Metric metric, std::size_t flush_threshold_rows)
    : dir_(std::move(dir)), metric_(metric), memtable_(dimensions, flush_threshold_rows) {}

bool SegmentStore::dir_opened() const noexcept {
    return !dir_.empty();
}

bool SegmentStore::needs_flush() const noexcept {
    return memtable_.needs_flush();
}

std::uint64_t SegmentStore::next_segment_id() const {
    std::uint64_t max_id = 0;
    for (const auto& name : manifest_.segments()) {
        max_id = std::max(max_id, parse_segment_id(name));
    }
    return max_id + 1;
}

std::string SegmentStore::make_segment_name(std::uint64_t id) const {
    std::ostringstream oss;
    oss << "segment-" << std::setw(6) << std::setfill('0') << id << ".vec";
    return oss.str();
}

Status SegmentStore::open() {
    if (dir_.empty()) {
        return Status::invalid_argument;
    }
    return manifest_.load(dir_);
}

Status SegmentStore::open(const std::string& dir) {
    if (dir.empty()) {
        return Status::invalid_argument;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return Status::invalid_argument;
    }
    dir_ = dir;
    return manifest_.load(dir_);
}

Status SegmentStore::maybe_auto_flush() {
    if (dir_.empty() || !memtable_.needs_flush()) {
        return Status::ok;
    }
    return flush();
}

Status SegmentStore::put(std::uint64_t id, const std::vector<float>& values) {
    Status st = memtable_.put(id, values);
    if (st != Status::ok) {
        return st;
    }
    return maybe_auto_flush();
}

Status SegmentStore::remove(std::uint64_t id) {
    memtable_.tombstone(id);
    return maybe_auto_flush();
}

Status SegmentStore::flush() {
    if (dir_.empty()) {
        return Status::invalid_argument;
    }
    if (memtable_.size() == 0) {
        return Status::ok;
    }

    const std::string segment_name = make_segment_name(next_segment_id());
    const std::string path = dir_ + "/" + segment_name;

    Status st = flush_memtable(memtable_, path, metric_);
    if (st != Status::ok) {
        return st;
    }

    Manifest next = manifest_;
    next.add(segment_name);
    st = next.replace(dir_);
    if (st != Status::ok) {
        return st;
    }
    manifest_.add(segment_name);
    return Status::ok;
}

Status SegmentStore::compact() {
    if (dir_.empty()) {
        return Status::invalid_argument;
    }
    if (manifest_.segments().size() < 2) {
        return Status::ok;
    }

    std::vector<std::string> old = manifest_.segments();
    std::unordered_map<std::uint64_t, std::vector<float>> live;
    std::unordered_set<std::uint64_t> seen;

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

    const std::string new_name = make_segment_name(next_segment_id());
    const std::string new_path = dir_ + "/" + new_name;
    SegmentWriter writer(new_path);

    if (writer.open(memtable_.dimensions(), metric_) != Status::ok) {
        return Status::invalid_argument;
    }

    for (const auto& [id, values] : live) {
        SegmentRow row;
        row.id = id;
        row.is_deleted = false;
        row.values = values;
        if (writer.append(row) != Status::ok) {
            return Status::invalid_argument;
        }
    }

    if (writer.finish() != Status::ok) {
        return Status::invalid_argument;
    }

    Manifest next;
    next.add(new_name);
    Status st = next.replace(dir_);
    if (st != Status::ok) {
        return Status::invalid_argument;
    }
    manifest_.clear();
    manifest_.add(new_name);

    for (const auto& old_name : old) {
        std::string old_path = dir_ + "/" + old_name;
        std::remove(old_path.c_str());
    }

    return Status::ok;
}

std::optional<std::vector<float>> SegmentStore::get(std::uint64_t id) const {
    if (auto e = memtable_.find(id)) {
        if (e->is_deleted) {
            return std::nullopt;
        }
        return e->values;
    }

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
            if (row.id != id) {
                continue;
            }
            if (row.is_deleted) {
                return std::nullopt;
            }
            return row.values;
        }
    }

    return std::nullopt;
}

std::vector<SearchResult> SegmentStore::search(const std::vector<float>& query, std::size_t k) const {
    if (k == 0 || query.size() != memtable_.dimensions()) {
        return {};
    }

    std::unordered_map<std::uint64_t, std::vector<float>> live;
    std::unordered_set<std::uint64_t> seen;

    for (const auto& [id, entry] : memtable_) {
        seen.insert(id);
        if (!entry.is_deleted) {
            live[id] = entry.values;
        }
    }

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

std::size_t SegmentStore::live_count() const {
    std::vector<float> dummy(memtable_.dimensions(), 0.f);
    return search(dummy, std::numeric_limits<std::size_t>::max()).size();
}

}  // namespace vectordb
