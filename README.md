# VectorDB From Scratch — Learning Journey

A small vector database in **C++20**, built step by step to learn the data structures and systems ideas behind real vector stores.

This is a **learning project**, not a production competitor to Faiss/Milvus/etc. The full curriculum lives in [`README_VectorDB_From_Scratch.md`](README_VectorDB_From_Scratch.md). This README is the **story of what I’ve built and learned**.

---

## Where things stand

```mermaid
flowchart LR
  D[Design] --> L[Memory layout]
  L --> I[ID index + CRUD]
  I --> S[Similarity]
  S --> K[Exact top-k]
  K --> P[Binary persistence]
  P --> W[WAL + recovery]
  W --> V02[Version 0.2]
  V02 --> SegFmt[Segment format]
  SegFmt --> Mem[Memtable + flush]
  Mem --> Man[MANIFEST]
  Man --> Read[SegmentStore read path]
  Read --> Comp[Compaction — next]
```

| Topic | Progress |
|-------|----------|
| Design | Locked: fixed dims, `float`, `uint64_t` ids, tombstones, `Status` |
| Memory layout | AoS + SoA stores; scan benchmark |
| ID index + CRUD | `IdIndex` + `VectorDB` insert/get/update/remove |
| Similarity | Cosine, dot, squared Euclidean |
| Exact top-k | Min-heap search · tag `v0.1-in-memory-exact` |
| Binary persistence | Save/load + checksum (`.vdb`) |
| WAL + recovery | Log-before-mutate, `fsync`, open/replay, checkpoint + CHECKPOINT record |
| Crash injection | Fork harness + matrix (process-crash scope) |
| **Version 0.2** | **Milestone 6 complete** |
| Segment file format | `VECSEG01` writer/reader + sandbox + tests (M7 #9) |
| Memtable + flush | `std::map` memtable, row-count threshold, `flush_memtable` (M7 #10–#11) |
| MANIFEST | `VECMAN01` load + temp/fsync/rename replace (M7 #12) |
| SegmentStore read path | Newest-wins `get` / top-k `search`; LSM `tombstone` delete (M7 #13) |
| Checkpoint vs segment I/O | Microbench: flush batch ≪ full `.vdb` rewrite |
| Tests | **141/141** CTest passing |
| Compaction | **Next** (M7 #15); #14 tombstones across segments largely covered by #13 |

```mermaid
pie title C++ lines by area (~3,900 total)
  "tests" : 1502
  "src" : 1329
  "tools" : 438
  "include" : 390
  "benchmarks" : 237
```

---

## Architecture

Version **0.2** (live path): one in-memory SoA store + `.vdb` snapshot + `.wal`.  
Milestone **7** (building beside it): memtable → flush → MANIFEST → `SegmentStore` read path done; compaction next — not wired into `VectorDB` yet.

```mermaid
flowchart TB
  subgraph API["Public API — VectorDB (0.2)"]
    insert["insert / update / remove / get"]
    search["search(query, k)"]
    save["save / load"]
    open["open / checkpoint"]
  end

  subgraph Memory["In-memory state"]
    store["FlatVectorStore\n(SoA layout)"]
    index["IdIndex\nid → position"]
    meta["metric_ + active_count_"]
    walptr["optional WalWriter"]
  end

  subgraph Disk["On disk — 0.2"]
    snap[".vdb snapshot"]
    wallog[".wal append-only log"]
  end

  subgraph M7["Milestone 7 — beside 0.2"]
    seglib["SegmentWriter / SegmentReader"]
    mt["Memtable"]
    man["Manifest VECMAN01"]
    ss["SegmentStore\nnewest-wins get/search"]
    segfile["segment-*.vec"]
  end

  insert --> walptr
  insert --> index
  insert --> store
  search --> store
  save --> snap
  open --> snap
  open --> wallog
  open --> store
  checkpoint --> snap
  checkpoint --> wallog
  mt -->|"flush"| seglib
  seglib --> segfile
  ss --> mt
  ss --> man
  ss --> seglib
  man -.-> segfile
```

---

## Design

**What I learned:** A database starts as a contract, not as code. Before touching C++, we locked the product rules — fixed dimensions, `float` embeddings, `uint64_t` ids, tombstone deletes, `Status` for expected failures. That mirrors how real systems separate the **logical API** from storage engines (same idea as “library first” in CMU-style DB courses: define the interface, then swap implementations).

**Resources / ideas:** treat Version 0.1 as an **exact in-memory** engine so later approximate indexes (HNSW) have a ground-truth baseline. Embeddings are points in ℝᵈ — the DB’s job is store, address, compare, and rank them.

```cpp
enum class Metric { cosine, dot_product, euclidean };

enum class Status {
    ok,
    duplicate_id,
    not_found,
    dimension_mismatch,
    invalid_argument
};

struct SearchResult {
    std::uint64_t id;
    float score;  // higher is better for every metric
};

class VectorDB {
public:
    explicit VectorDB(std::size_t dimensions, Metric metric = Metric::cosine);

    Status insert(std::uint64_t id, std::span<const float> values);
    Status update(std::uint64_t id, std::span<const float> values);
    Status remove(std::uint64_t id);
    std::optional<std::span<const float>> get(std::uint64_t id) const;
    std::vector<SearchResult> search(std::span<const float> query, std::size_t k) const;
    // ...
};
```

---

## Memory layout

**What I learned:** Databases care how data sits in RAM, not just what it means. Version A (AoS) is simple — each row owns its own `vector<float>`. Version B (SoA) puts all floats in one slab so a full scan walks memory sequentially. From *[What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)*: CPUs hate pointer chasing; **cache lines and DRAM bandwidth** dominate once vectors get wide/large.

**Benchmark takeaway:** at 128-d × 100k, the flat SoA layout is typically ~1.1–1.3× faster than AoS (run-to-run variance); at ~512 MB of floats both hit DRAM bandwidth and the gap shrinks. Also learned that benchmarks lie if you discard results (`-O3` dead-code-eliminates the scan) or run unoptimized builds.

```text
Version A:  [Rec][Rec][Rec] → each Rec points elsewhere to floats
Version B:  ids_ | deleted_ | values_ = [v0|v1|v2|...] contiguous
```

```cpp
// FlatVectorStore — structure-of-arrays
class FlatVectorStore {
public:
    explicit FlatVectorStore(std::size_t dimensions);
    std::optional<std::size_t> append(std::uint64_t id, std::span<const float> values);
    std::span<const float> values_at(std::size_t position) const;
    // ...
private:
    std::size_t dimensions_;
    std::vector<float> values_;       // one contiguous slab
    std::vector<std::uint64_t> ids_;
    std::vector<bool> deleted_;
};
```

---

## ID index and CRUD

**What I learned:** A classic DB split — **primary storage** vs **secondary index**. The store addresses rows by position (great for scans); users speak ids. The index is a hash map `id → position` (same role as a heap-file + hash index in a textbook engine). We skipped building open-addressing from scratch (already knew hashing from [Stanford CS166](https://web.stanford.edu/class/archive/cs/cs166/cs166.1256/lectures/04/) / [MIT 6.006](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-fall-2011/resources/lecture-8-hashing-with-chaining/)) and used `unordered_map` so we could move on.

**Database idea:** deletes are **tombstones** — mark deleted, leave the slot — so positions stay stable and we don’t reshuffle the float slab on every remove. Physical reclaim can come later (compaction).

```cpp
class IdIndex {
    // id → position in FlatVectorStore
    bool insert(std::uint64_t id, std::size_t position);
    std::optional<std::size_t> find(std::uint64_t id) const;
    bool erase(std::uint64_t id);
private:
    std::unordered_map<std::uint64_t, std::size_t> map_;
};
```

```cpp
Status VectorDB::insert(std::uint64_t id, std::span<const float> values) {
    if (values.size() != store_.dimensions()) { return Status::dimension_mismatch; }
    if (index_.find(id) != std::nullopt) { return Status::duplicate_id; }
    auto pos = store_.append(id, values);
    if (!pos) { return Status::dimension_mismatch; }
    index_.insert(id, *pos);
    ++active_count_;
    return Status::ok;
}

Status VectorDB::remove(std::uint64_t id) {
    auto pos = index_.find(id);
    if (!pos) { return Status::not_found; }
    store_.set_deleted(*pos, true);   // tombstone — slot stays
    index_.erase(id);
    --active_count_;
    return Status::ok;
}
```

---

## Similarity

**What I learned:** Vector DBs don’t run SQL `WHERE` — they rank by **geometry in embedding space**. [3Blue1Brown on dot products](https://www.3blue1brown.com/lessons/dot-products) makes the intuition stick: dot product measures “how aligned”; cosine normalizes length so magnitude doesn’t dominate; Euclidean is literal distance. From the [Floating Point Guide](https://floating-point-gui.de/): floats are approximate — tests use tolerances, and we avoid useless work (no `√` when only ranking order matters).

**Database idea:** pick one **score convention** (higher = better) at the API boundary so search/heap code doesn’t special-case every metric. Euclidean becomes `-squared_distance` for ranking.

```cpp
float dot_product(std::span<const float> a, std::span<const float> b) {
    assert(a.size() == b.size());
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

float cosine_similarity(std::span<const float> a, std::span<const float> b) {
    float dot = dot_product(a, b);
    float norm_a = std::sqrt(dot_product(a, a));
    float norm_b = std::sqrt(dot_product(b, b));
    if (norm_a == 0.0f || norm_b == 0.0f) { return 0.0f; }
    return dot / (norm_a * norm_b);
}

float squared_euclidean(std::span<const float> a, std::span<const float> b) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return sum;  // no sqrt — ranking only needs order
}
```

```cpp
float VectorDB::score_pair(std::span<const float> query,
                           std::span<const float> candidate) const {
    switch (metric_) {
        case Metric::cosine:      return cosine_similarity(query, candidate);
        case Metric::dot_product: return dot_product(query, candidate);
        case Metric::euclidean:   return -squared_euclidean(query, candidate);
    }
}
```

---

## Exact top-k search

**What I learned:** Exact search = score **every** live vector — slow but correct. That’s the ground truth Faiss-style ANN will approximate later. From [MIT heaps](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-fall-2011/resources/lecture-4-heaps-and-heap-sort/) / [`priority_queue`](https://en.cppreference.com/w/cpp/container/priority_queue): you don’t need to sort all *n* scores to get top-k — keep a size-*k* heap of the best so far (`O(n log k)` after the distance work).

**Database idea:** scan the heap file (our SoA store), skip tombstones, push `(id, score)`, pop when over capacity. Comparator compares **scores only**; id is payload. Complexity: `O(n·d)` distances + `O(n log k)` heap.

```cpp
struct WorseFirst {
    bool operator()(const SearchResult& a, const SearchResult& b) const {
        return a.score > b.score;  // lowest score sits on top
    }
};

std::vector<SearchResult> VectorDB::search(std::span<const float> query,
                                           std::size_t k) const {
    if (query.size() != dimensions() || k == 0) { return {}; }

    std::priority_queue<SearchResult, std::vector<SearchResult>, WorseFirst> heap;
    for (std::size_t i = 0; i < store_.size(); ++i) {
        if (store_.is_deleted(i)) { continue; }
        float score = score_pair(query, store_.values_at(i));
        heap.push({store_.id_at(i), score});
        if (heap.size() > k) { heap.pop(); }
    }

    std::vector<SearchResult> results;
    while (!heap.empty()) {
        results.push_back(heap.top());
        heap.pop();
    }
    std::reverse(results.begin(), results.end());  // best first
    return results;
}
```

Tag: **`v0.1-in-memory-exact`**.

---

## Binary persistence

**What I learned:** Memory dies when the process exits — a real DB needs an **on-disk format**. Studying the [SQLite file format](https://www.sqlite.org/fileformat.html) made the pattern clear: **magic** (“is this our file?”), **version** (“which layout rules?”), explicit fixed-width fields, then payload. You don’t `fwrite` a C++ struct and hope — padding and endianness aren’t portable. Fixed-width integers ([cppreference](https://en.cppreference.com/w/cpp/types/integer)) are the contract.

**Database ideas:**
- Persist the **table** (SoA rows), not the hash index — indexes are rebuildable.
- Checksum = cheap corruption detector (not crypto).
- Load must **validate then rebuild**; never trust a half-written file.
- Tombstones go on disk too, or physical layout won’t round-trip.

```mermaid
flowchart LR
  subgraph Save
    M1["memory DB"] --> W1["header"]
    W1 --> W2["ids / deleted / floats"]
    W2 --> W3["checksum"]
    W3 --> F["file.vdb"]
  end

  subgraph Load
    F2["file.vdb"] --> R1["validate header"]
    R1 --> R2["read payload"]
    R2 --> R3["verify checksum"]
    R3 --> R4["rebuild DB"]
    R4 --> M2["memory DB"]
  end
```

```text
[ magic 8 ][ ver 4 ][ dims 4 ][ n 8 ][ active 8 ][ metric 4 ]  = 36 B header
[ ids 8n ][ deleted 1n ][ floats 4nd ][ checksum 4 ]
```

```cpp
inline constexpr char kMagic[8] = {'V','E','C','D','B','0','0','1'};
inline constexpr std::uint32_t kFormatVersion = 1;
```

```cpp
Status save_database(const VectorDB& db, const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) { return Status::invalid_argument; }

    std::uint32_t checksum = 0;
    write_header(file, db, checksum);
    write_payload(file, db, checksum);
    std::fwrite(&checksum, 4, 1, file);  // not part of the sum
    std::fclose(file);
    return Status::ok;
}
```

```cpp
// SoA payload — three sections, not one record per row
void write_payload(FILE* file, const VectorDB& db, std::uint32_t& checksum) {
    const std::size_t n = db.physical_size();

    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t id = db.id_at(i);
        fwrite(&id, 8, 1, file);
        add_bytes(checksum, &id, 8);
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::uint8_t d = db.is_deleted_at(i) ? 1 : 0;  // not bool
        fwrite(&d, 1, 1, file);
        add_bytes(checksum, &d, 1);
    }
    for (std::size_t i = 0; i < n; ++i) {
        auto values = db.values_at(i);
        fwrite(values.data(), 4, values.size(), file);
        add_bytes(checksum, values.data(), values.size() * 4);
    }
}
```

```cpp
// After checksum matches — rebuild from file buffers
db = VectorDB(dimensions, metric_from_u32(metric));
for (std::size_t i = 0; i < record_count; ++i) {
    std::span<const float> vals(values.data() + i * dimensions, dimensions);
    db.insert(ids[i], vals);
    if (deleted[i]) { db.remove(ids[i]); }  // restore tombstone
}
```

**Hard lessons from implementing it:** `bool` isn’t an on-disk type; `fwrite(ptr, size, count)` — size is bytes per item; don’t `strlen` an 8-byte magic; load trusts the **file**, not whatever was already in `db`.

---

## Write-ahead log + recovery

**What I learned:** A `.vdb` snapshot is a **photo** of full state; a WAL is a **notebook** of changes since that photo. Studying [SQLite WAL](https://www.sqlite.org/wal.html) and CMU-style recovery: durability comes from append-only log records that hit disk **before** you mutate RAM. On open: load last snapshot (if any), replay every WAL record, then reopen the writer at `max_lsn + 1`. Checkpoint = save photo, close writer, truncate WAL, reopen at LSN 1.

**Database ideas:**
- **Write-ahead:** validate → append WAL → mutate memory (failed ops write nothing).
- **LSN:** monotonic id so the next writer knows where to continue after replay.
- **WAL vs snapshot:** WAL = cheap per-op durability; checkpoint keeps replay short.
- Incomplete types: forward-declare `WalWriter` in `database.hpp` so headers don’t circular-include.

```mermaid
flowchart LR
  op["insert"] --> wal["append WAL + flush"]
  wal --> mem["update RAM"]
  mem --> cp["checkpoint → .vdb + truncate WAL"]
  open["open"] --> snap["load .vdb"]
  snap --> replay["replay .wal"]
  replay --> writer["enable_wal @ next LSN"]
```

```text
[ record_length u32 ]  = bytes of (lsn + op + payload + checksum)
[ lsn           u64 ]
[ op_type       u32 ]  1=INSERT 2=UPDATE 3=DELETE 4=CHECKPOINT
[ payload... ]
[ checksum      u32 ]  // lsn+op+payload only
```

```cpp
Status VectorDB::open(const std::string& snapshot_path,
                      const std::string& wal_path) {
    // if snapshot exists → load()
    // replay_wal → out_max_lsn = max + 1
    // enable_wal(wal_path); wal_->set_next_lsn(out_max_lsn)
}

Status VectorDB::checkpoint(const std::string& snapshot_path) {
    // save(snapshot);
    // append CHECKPOINT{checkpoint_lsn} + flush;
    // wal_.reset(); truncate wal_path_; enable_wal(...)
}
```

**Hard lessons so far:** `fread` return value ≠ `record_length`; close the writer (`wal_.reset()`) *before* truncating the file; don’t call `~WalWriter()` by hand; store `wal_path_`; replay must be idempotent when snapshot + old WAL overlap; `fwrite` may already be in the kernel before `fflush` on some platforms — documented durable point is still `fflush` + `fsync`.

**Milestone 6 complete:** fsync, crash hooks + fork harness/matrix (6 crash points), CHECKPOINT record before truncate.

---

## Segments (LSM path — in progress)

**What I learned:** A `.vdb` checkpoint is a full **rewrite** — cost grows with **total** `N`, even if you only inserted a few rows since the last save. LSM-style storage flips that: keep a small mutable **memtable**, and when it fills, **flush** only that batch into a new immutable **segment** file. Old segments stay untouched; updates/deletes become newer rows or tombstones (newest-wins). Compaction merges later to reclaim garbage.

**Database ideas:**
- Checkpoint (0.2) = O(N) bytes + O(N) work per save.
- Segment flush = O(batch) — write amplification tracks recent writes, not history.
- Segments are **immutable** after `finish()` — simplifies crash safety and future concurrent readers.
- **MANIFEST** is the source of truth for live segment files (never “glob `*.vec`”).
- Build the LSM stack **beside** the live `.vdb` + WAL path; wire into `VectorDB` later.

```mermaid
flowchart LR
  subgraph today["Version 0.2 — live"]
    ram["FlatVectorStore"] --> vdb["rewrite .vdb"]
  end

  subgraph m7["Milestone 7 — beside 0.2"]
    mt["Memtable"] -->|"flush"| seg["segment-NNNNNN.vec"]
    seg --> man["MANIFEST"]
    ss["SegmentStore"] --> mt
    ss --> man
    ss --> seg
  end
```

```text
[ magic 8 ]  'V','E','C','S','E','G','0','1'
[ version u32 ][ dimensions u32 ][ record_count u64 ][ metric u32 ]
[ ids u64×n ][ deleted u8×n ][ floats f32×n×d ]
[ checksum u32 ]
```

**Pieces shipped (#8–#13):**
- `SegmentWriter` / `SegmentReader` (`VECSEG01`)
- `Memtable` (`std::map`, row-count `needs_flush`, `tombstone`)
- `flush_memtable` → new `.vec` then clear
- `Manifest` (`VECMAN01`, temp + `fsync` + rename)
- `SegmentStore`: put/remove/flush/open + newest-wins `get` + O(n log k) `search`

**Hard lesson — LSM delete:** after flush the id is gone from the memtable. `Memtable::remove` would return `not_found`. `SegmentStore::remove` must `tombstone(id)` so a newer delete marker hides older segment rows.

**Benchmark takeaway** (`checkpoint_vs_segment_benchmark`): at **128-d × 100K** vectors, a full `.vdb` checkpoint wrote **~52 MB** in **~0.079s**; flushing a **1K-row** segment wrote **~0.52 MB** in **~0.001s** — about **76×** faster and **100×** fewer bytes for that batch.

**Milestone 7 status:** tickets **#8–#13** done. **Next:** #14 harden / document cross-segment tombstones if needed, then **#15 compaction**.  
Detail: [`notes/07-segments-compaction.md`](notes/07-segments-compaction.md).

---

## Repo layout

```text
include/vectordb/   public headers (database, wal, segment, memtable, manifest, segment_store, …)
src/                implementations
tests/              GoogleTest (141 cases)
tools/              CLI + format / wal / segment sandboxes
benchmarks/         AoS vs SoA scans + checkpoint vs segment I/O
notes/              design, memory-layout, WAL, segments learning notes
```

---

## Build & test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

```bash
./build/vectordb_cli
./build/format_sandbox
./build/wal_sandbox
./build/segment_sandbox
./build/storage_layout_benchmark
./build/checkpoint_vs_segment_benchmark
```

---

## How I learn

1. **Read** — short notes in my own words  
2. **Draw** — layout / data flow  
3. **Implement** — small steps; tests for behavior  
4. **Benchmark** when layout or speed claims matter  
5. **Reflect** — what broke, what to redesign  

**Version 0.2 done** (Milestone 6). **Milestone 7:** #8–#13 complete (segments → memtable → MANIFEST → `SegmentStore`); compaction next.  
Notes: [`notes/06-wal-learning.md`](notes/06-wal-learning.md) · [`notes/07-segments-compaction.md`](notes/07-segments-compaction.md) · Curriculum: [`README_VectorDB_From_Scratch.md`](README_VectorDB_From_Scratch.md).

---

## License

Personal / educational use unless otherwise stated.
