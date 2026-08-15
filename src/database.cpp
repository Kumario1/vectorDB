#include "vectordb/database.hpp"
#include "vectordb/crash.hpp"
#include "vectordb/distance.hpp"
#include "vectordb/segment_store.hpp"
#include "vectordb/serializer.hpp"
#include "vectordb/wal.hpp"
#include <memory>

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <queue>
#include <span>

namespace vectordb {


Status VectorDB::enable_wal(const std::string& path) {
    if (is_lsm()) {
        return Status::invalid_argument;
    }
    auto w = std::make_unique<WalWriter>(path);
    Status status = w->open();
    if (status != Status::ok) {
        return status;
    }
    wal_ = std::move(w);
    wal_path_ = path;
    return Status::ok;
}

VectorDB::~VectorDB() = default;

VectorDB::VectorDB(VectorDB&&) noexcept = default;
VectorDB& VectorDB::operator=(VectorDB&&) noexcept = default;

VectorDB::VectorDB(std::size_t dimensions, Metric metric, StorageMode mode,
                   std::size_t flush_threshold_rows)
    : mode_(mode), metric_(metric), store_(dimensions) {
    if (mode_ == StorageMode::lsm) {
        segments_ = std::make_unique<SegmentStore>("", dimensions, metric, flush_threshold_rows);
    }
}

Status VectorDB::save(const std::string& path) const {
    if (is_lsm()) {
        return Status::invalid_argument;
    }
    return save_database(*this, path);
}

Status VectorDB::load(const std::string& path) {
    if (is_lsm()) {
        return Status::invalid_argument;
    }
    return load_database(path, *this);
}

Status VectorDB::open(const std::string& snapshot_path, const std::string& wal_path) {
    if (is_lsm()) {
        return Status::invalid_argument;
    }
    uint64_t out_max_lsn = 0;
    if (!snapshot_path.empty() && std::filesystem::exists(snapshot_path)) {
        Status st = load(snapshot_path);
        if (st != Status::ok) {return st;}
    }
    if (!wal_path.empty()) {
        Status st = replay_wal(wal_path, out_max_lsn);
        if (st != Status::ok) {return st;}
        st = enable_wal(wal_path);
        if (st != Status::ok) {return st;}
        wal_->set_next_lsn(out_max_lsn);
    }
    return Status::ok;
}

Status VectorDB::checkpoint(const std::string& snapshot_path) {
    if (is_lsm()) {
        return Status::invalid_argument;
    }
    if (wal_){
        Status st = save(snapshot_path);
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterCheckpointSnapshot);
        //create wal record for checkpoitn
        WalRecord rec;
        rec.op = WalOp::Checkpoint;
        rec.checkpoint_lsn = wal_->next_lsn() - 1;
        st = wal_->append(rec);
        if (st != Status::ok) {return st;}
        st = wal_->flush();
        if (st != Status::ok) {return st;}

        std::string wal_path = wal_path_;
        wal_.reset();
        maybe_crash(CrashPoint::AfterCheckpointBeforeTruncateWal);

        std::ofstream ofs(wal_path, std::ios::trunc);
        ofs.close();

        st = enable_wal(wal_path);
        if (st != Status::ok) {return st;}
    }
    return Status::ok;
}

Status VectorDB::open_lsm(const std::string& dir) {
    if (!is_lsm() || dir.empty() || !segments_) {
        return Status::invalid_argument;
    }
    Status st = segments_->open(dir);
    if (st != Status::ok) {
        return st;
    }
    active_count_ = segments_->live_count();
    return Status::ok;
}

Status VectorDB::flush() {
    if (!is_lsm() || !segments_) {
        return Status::invalid_argument;
    }
    if (!segments_->dir_opened()) {
        return Status::ok;  // no-op until open_lsm
    }
    return segments_->flush();
}

Status VectorDB::compact() {
    if (!is_lsm() || !segments_) {
        return Status::invalid_argument;
    }
    if (!segments_->dir_opened()) {
        return Status::invalid_argument;
    }
    return segments_->compact();
}

Status VectorDB::apply_wal_record(const WalRecord& rec) {
    // Replay must be idempotent: snapshot + untruncated WAL can re-apply
    // ops already reflected in the loaded .vdb.
    switch (rec.op) {
        case WalOp::Insert: {
            Status st = insert(rec.id, rec.values);
            return (st == Status::duplicate_id) ? Status::ok : st;
        }
        case WalOp::Update: {
            Status st = update(rec.id, rec.values);
            if (st != Status::not_found) {return st;}
            st = insert(rec.id, rec.values);
            return (st == Status::duplicate_id) ? Status::ok : st;
        }
        case WalOp::Delete: {
            Status st = remove(rec.id);
            return (st == Status::not_found) ? Status::ok : st;
        }
        case WalOp::Checkpoint: return Status::ok;
        default: return Status::invalid_argument;
    }
}

Status VectorDB::replay_wal(const std::string& wal_path, uint64_t& out_max_lsn) {
    auto w = std::make_unique<WalReader>(wal_path);
    Status st = w->open();
    if (st != Status::ok) {return st;}
    while (true) {
        WalRecord rec;
        bool have_record = false;
        st = w->read_next(rec, have_record);
        //if fails, return the status
        if (st != Status::ok) {return st;}
        if (!have_record) {break;}
        //use apply_wal_record to apply the record
        st = apply_wal_record(rec);
        if (st != Status::ok) {return st;}
        if (rec.lsn > out_max_lsn) {out_max_lsn = rec.lsn;}
    }
    out_max_lsn++;
    return Status::ok;
}

Status VectorDB::lsm_insert(std::uint64_t id, std::span<const float> values) {
    if (values.size() != store_.dimensions()) {
        return Status::dimension_mismatch;
    }
    if (segments_->get(id)) {
        return Status::duplicate_id;
    }
    Status st = segments_->put(id, std::vector<float>(values.begin(), values.end()));
    if (st != Status::ok) {
        return st;
    }
    ++active_count_;
    return Status::ok;
}

Status VectorDB::lsm_update(std::uint64_t id, std::span<const float> values) {
    if (values.size() != store_.dimensions()) {
        return Status::dimension_mismatch;
    }
    if (!segments_->get(id)) {
        return Status::not_found;
    }
    return segments_->put(id, std::vector<float>(values.begin(), values.end()));
}

Status VectorDB::lsm_remove(std::uint64_t id) {
    if (!segments_->get(id)) {
        return Status::not_found;
    }
    Status st = segments_->remove(id);
    if (st != Status::ok) {
        return st;
    }
    --active_count_;
    auto mit = metadata_.find(id);
    if (mit != metadata_.end()) {
        eq_index_.remove(id, mit->second);
        metadata_.erase(mit);
    }
    return Status::ok;
}

Status VectorDB::insert(std::uint64_t id, std::span<const float> values) {
    if (is_lsm()) {
        return lsm_insert(id, values);
    }
    if (values.size() != store_.dimensions()) {return Status::dimension_mismatch;}
    if (index_.find(id) != std::nullopt) {return Status::duplicate_id;}
    if (wal_){
        WalRecord rec;
        rec.op = WalOp::Insert;
        rec.id = id;
        rec.dimensions = store_.dimensions();
        rec.values.assign(values.begin(), values.end());
        maybe_crash(CrashPoint::BeforeWalAppend);
        Status st = wal_->append(rec);
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalAppendBeforeFlush);
        st = wal_->flush();
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalFlush);
    }
    auto pos = store_.append(id, values);
    if (!pos) {return Status::dimension_mismatch;}
    index_.insert(id, *pos);
    ++active_count_;
    maybe_crash(CrashPoint::AfterMemoryApply);
    return Status::ok;
}

Status VectorDB::insert(std::uint64_t id, std::span<const float> values, const Metadata& metadata) {
    Status st = insert(id, values);
    if (st != Status::ok) {return st;}
    if (metadata.empty()) {
        auto mit = metadata_.find(id);
        if (mit != metadata_.end()) {
            eq_index_.remove(id, mit->second);
            metadata_.erase(mit);
        }
        return Status::ok;
    }
    metadata_[id] = metadata;
    eq_index_.add(id, metadata);
    return Status::ok;
}

Status VectorDB::set_metadata(std::uint64_t id, const Metadata& metadata) {
    if (!get(id).has_value()) {
        return Status::not_found;
    }
    Metadata old;
    auto mit = metadata_.find(id);
    if (mit != metadata_.end()) {
        old = mit->second;
    }
    if (metadata.empty()) {
        if (!old.empty()) {
            eq_index_.remove(id, old);
        }
        metadata_.erase(id);
        return Status::ok;
    }
    if (old.empty()) {
        eq_index_.add(id, metadata);
    } else {
        eq_index_.update(id, old, metadata);
    }
    metadata_[id] = metadata;
    return Status::ok;
}

Status VectorDB::update(std::uint64_t id, std::span<const float> values) {
    if (is_lsm()) {
        return lsm_update(id, values);
    }
    // 1) wrong dims → dimension_mismatch
    // 2) auto pos = index_.find(id); if !pos → not_found
    // 3) if store_.is_deleted(*pos) → not_found  (defensive)
    // 4) copy values into store_.values_at(*pos)  (same length)
    // 5) return ok
    if (values.size() != store_.dimensions()) {return Status::dimension_mismatch;}
    auto pos = index_.find(id);
    if (!pos) {return Status::not_found;}
    if (wal_){
        WalRecord rec;
        rec.op = WalOp::Update;
        rec.id = id;
        rec.dimensions = store_.dimensions();
        rec.values.assign(values.begin(), values.end());
        maybe_crash(CrashPoint::BeforeWalAppend);
        Status st = wal_->append(rec);
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalAppendBeforeFlush);
        st = wal_->flush();
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalFlush);
    }
    if (store_.is_deleted(*pos)) {return Status::not_found;}
    auto dest = store_.values_at(*pos);
    std::copy(values.begin(), values.end(), dest.begin());
    maybe_crash(CrashPoint::AfterMemoryApply);
    return Status::ok;
}

Status VectorDB::remove(std::uint64_t id) {
    if (is_lsm()) {
        return lsm_remove(id);
    }
    // 1) auto pos = index_.find(id); if !pos → not_found
    // 2) store_.set_deleted(*pos, true)
    // 3) index_.erase(id)
    // 4) --active_count_
    // 5) return ok
    auto pos = index_.find(id);
    if (!pos) {return Status::not_found;}
    if (wal_){
        WalRecord rec;
        rec.op = WalOp::Delete;
        rec.id = id;
        maybe_crash(CrashPoint::BeforeWalAppend);
        Status st = wal_->append(rec);
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalAppendBeforeFlush);
        st = wal_->flush();
        if (st != Status::ok) {return st;}
        maybe_crash(CrashPoint::AfterWalFlush);
    }
    if (store_.is_deleted(*pos)) {return Status::not_found;}
    store_.set_deleted(*pos, true);
    index_.erase(id);
    auto mit = metadata_.find(id);
    if (mit != metadata_.end()) {
        eq_index_.remove(id, mit->second);
        metadata_.erase(mit);
    }
    --active_count_;
    maybe_crash(CrashPoint::AfterMemoryApply);
    return Status::ok;
}

std::optional<std::span<const float>> VectorDB::get(std::uint64_t id) const {
    if (is_lsm()) {
        auto v = segments_->get(id);
        if (!v) {
            return std::nullopt;
        }
        get_scratch_ = std::move(*v);
        return std::span<const float>(get_scratch_);
    }
    // 1) find pos in index_; if missing → nullopt
    // 2) if store_.is_deleted(*pos) → nullopt
    // 3) return store_.values_at(*pos)
    auto pos = index_.find(id);
    if (!pos) {return std::nullopt;}
    if (store_.is_deleted(*pos)) {return std::nullopt;}
    return store_.values_at(*pos);
}

std::optional<Metadata> VectorDB::get_metadata(std::uint64_t id) const {
    //first check if the id is in the DB
    if(!get(id)) {return std::nullopt;}
    //then check if the metadata is in the metadata_ map
    auto it = metadata_.find(id);
    if (it == metadata_.end()) return Metadata{};
    return it->second;
}

PostingList VectorDB::lookup(std::string_view field, const MetadataValue& value) const {
    return eq_index_.lookup(field, value);
}

std::size_t VectorDB::dimensions() const noexcept {
    // return store_.dimensions()
    return store_.dimensions();
}

Metric VectorDB::metric() const noexcept {
    // return metric_
    return metric_;
}

std::size_t VectorDB::size() const noexcept {
    // return active_count_
    return active_count_;
}

StorageMode VectorDB::storage_mode() const noexcept {
    return mode_;
}

std::size_t VectorDB::physical_size() const noexcept {
    if (is_lsm()) {
        return 0;
    }
    return store_.size();
}

std::uint64_t VectorDB::id_at(std::size_t index) const noexcept {
    if (is_lsm()) {
        return 0;
    }
    return store_.id_at(index);
}

bool VectorDB::is_deleted_at(std::size_t index) const noexcept {
    if (is_lsm()) {
        return true;
    }
    return store_.is_deleted(index);
}

std::span<const float> VectorDB::values_at(std::size_t index) const noexcept {
    if (is_lsm()) {
        return {};
    }
    return store_.values_at(index);
}

float VectorDB::score_pair(std::span<const float> query, std::span<const float> candidate) const {
    assert(query.size() == candidate.size());
    switch (metric_) {
        case Metric::cosine: return cosine_similarity(query, candidate);
        case Metric::dot_product: return dot_product(query, candidate);
        case Metric::euclidean: return -squared_euclidean(query, candidate);
    }
}

struct WorseFirst {
    bool operator()(const SearchResult& a, const SearchResult& b) const {
        return a.score > b.score;  // higher score = "less" → lowest score on top
    }
};

std::vector<SearchResult> VectorDB::search(std::span<const float> query, std::size_t k) const {
    if (is_lsm()) {
        std::vector<float> q(query.begin(), query.end());
        return segments_->search(q, k);
    }
    if (query.size() != dimensions()) {return {};}
    if (k == 0) {return {};}
    //min heap of all SearchResults by score (worst on top)
    std::priority_queue<SearchResult, std::vector<SearchResult>, WorseFirst> heap;

    for (std::size_t i = 0; i < store_.size(); ++i) {
        if (store_.is_deleted(i)) {continue;}
        float score = score_pair(query, store_.values_at(i));
        heap.push({store_.id_at(i), score});
        if (heap.size() > k) {heap.pop();}
    }

    std::vector<SearchResult> results;
    while (!heap.empty()) {
        results.push_back(heap.top());
        heap.pop();
    }
    
    std::reverse(results.begin(), results.end());
    return results;
    // 1) if query.size() != dimensions() → return {}  (or we add Status later)
    // 2) if k == 0 → return {}
    // 3) min-heap of SearchResult by score (worst on top)
    //    hint: priority_queue with comparator where a.score > b.score means a is "lower priority"
    // 4) for i in [0, store_.size()): skip deleted; score; push; if size>k pop
    // 5) pop all into a vector, reverse so best is first
}

}  // namespace vectordb
