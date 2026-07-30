# Milestone 0 — Short Design Document

Version 0.1 target: a correct **in-memory exact** vector database.
The core is a **library first**; CLI and HTTP come later as wrappers around the same API.

---

## Assumptions

| Decision | Choice |
|----------|--------|
| Dimensions | Fixed at construction |
| Component type | `float` (`float32`) |
| ID type | `uint64_t` |
| Duplicate insert | Error (use `update` to replace) |
| Metrics | Cosine (default), dot product, Euclidean |
| Deletion | Tombstone now; physical reclaim in Milestone 7 |
| Errors | Status for expected failures; `assert` for invariant bugs |
| Threading | Single-threaded for Version 0.1 |
| Persistence | In-memory only for Version 0.1 |
| Score convention | Higher is better for all metrics (Euclidean returns negated distance) |

---

## Public API (Version 0.1)

```cpp
enum class Metric { cosine, dot_product, euclidean };

enum class Status {
    ok,
    duplicate_id,
    not_found,
    dimension_mismatch,
    invalid_argument  // e.g. k == 0
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

    // Empty optional if missing or tombstoned
    std::optional<std::span<const float>> get(std::uint64_t id) const;

    // Exact top-k; skips tombstones; uses the DB metric
    std::vector<SearchResult> search(std::span<const float> query, std::size_t k) const;

    std::size_t dimensions() const noexcept;
    Metric metric() const noexcept;
    std::size_t size() const noexcept;  // active (non-deleted) count
};
```

### Behavior notes

- `insert` rejects wrong dimension (`dimension_mismatch`) and existing IDs (`duplicate_id`).
- `update` / `remove` / `get` return `not_found` (or empty optional for `get`) when the ID is missing or tombstoned.
- `remove` sets a tombstone; the row stays in storage until later compaction.
- `search` is exact brute-force over active vectors; no approximate index in v0.1.
- Metadata on insert is deferred to Milestone 8 (not part of this API yet).

---

## Non-goals (Version 0.1)

Version 0.1 will **not** include:

- Persistence / save-load (Milestone 5+)
- WAL / crash recovery
- Metadata or filtered search
- Approximate search (HNSW, KD-tree)
- Concurrency
- Quantization / compression
- Network / HTTP API (Milestone 20; library stays usable without a server)
- Variable dimensions or string IDs
- Automatic embedding generation (models stay outside the DB)

### Later deployment path

1. Exact in-memory engine (v0.1)
2. Persistence + WAL
3. CLI (Milestone 19)
4. HTTP service as a thin wrapper (Milestone 20)
