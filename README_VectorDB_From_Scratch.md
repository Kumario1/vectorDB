# VectorDB From Scratch

Build a small but real vector database in C++ while learning the data structures and database concepts behind it.

This project is not meant to compete with production systems. Its purpose is to help you deeply understand:

- contiguous memory and dynamic arrays
- hashing and collision handling
- heaps and top-k selection
- vector similarity and nearest-neighbor search
- serialization and binary file formats
- write-ahead logging and crash recovery
- metadata indexes, bitmaps, and B+ trees
- graph-based approximate search with HNSW
- caching, probabilistic structures, concurrency, and benchmarking

By the end, the database should support:

```cpp
VectorDB db(/* dimensions = */ 384);

db.insert(101, embedding, {{"category", "book"}, {"year", "1987"}});
db.update(101, replacement_embedding);
db.remove(101);

auto record = db.get(101);
auto results = db.search(query_embedding, 10);
auto filtered = db.search(query_embedding, 10, "category = book");

db.save("data/my_database");
db.load("data/my_database");
```

---

## 1. Learning method

For every milestone, use this loop:

1. **Read:** Learn the idea and write a short explanation in your own words.
2. **Draw:** Sketch the data structure and trace one operation manually.
3. **Implement:** Avoid using a library that hides the main structure you are learning.
4. **Test:** Test normal input, edge cases, and invalid input.
5. **Benchmark:** Measure time and memory rather than guessing.
6. **Reflect:** Record what was slow, what broke, and what you would redesign.

Create a learning log:

```text
notes/
├── 01-memory-layout.md
├── 02-hash-table.md
├── 03-top-k-heap.md
├── 04-persistence.md
├── 05-wal-recovery.md
├── 06-metadata-indexes.md
└── 07-hnsw.md
```

Each note should answer:

- What problem does this structure solve?
- What are its invariants?
- What is the expected time complexity?
- What is the memory cost?
- What inputs make it perform poorly?
- Why is it useful in this database?

---

## 2. Recommended prerequisites

You should be comfortable with:

- C++ classes, references, pointers, templates, and RAII
- `std::vector`, `std::string`, and basic file I/O
- asymptotic time and space complexity
- binary trees, graphs, hash tables, and heaps at an introductory level
- basic unit testing and debugging

You do **not** need prior experience building a database.

### C++ refreshers

- [LearnCpp](https://www.learncpp.com/)
- [C++ reference](https://en.cppreference.com/w/)
- [CMake tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/)
- [GoogleTest primer](https://google.github.io/googletest/primer.html)

### Database foundations

- [CMU 15-445/645: Introduction to Database Systems](https://15445.courses.cs.cmu.edu/)
- [CMU Database Group courses](https://db.cs.cmu.edu/courses/)
- *Database System Concepts* by Silberschatz, Korth, and Sudarshan
- *Designing Data-Intensive Applications* by Martin Kleppmann

Do not try to finish an entire database textbook before starting. Read only the chapters needed for the current milestone.

---

## 3. Project rules

To get the full learning value:

- Use C++20 or newer.
- Implement the main educational structures yourself.
- Standard containers are allowed initially as a reference implementation.
- Keep the correct, simple version before adding optimizations.
- Benchmark every major optimization against the previous version.
- Do not add HNSW until exact brute-force search is correct.
- Do not add concurrency until the single-threaded version is reliable.
- Use assertions to protect data-structure invariants.

A useful pattern is to keep two implementations:

```text
Reference implementation → simple and obviously correct
Custom implementation    → educational and optimized
```

Compare their outputs after every operation.

---

## 4. Final architecture

```text
                         ┌──────────────────────┐
                         │      Public API      │
                         └──────────┬───────────┘
                                    │
             ┌──────────────────────┼──────────────────────┐
             │                      │                      │
             ▼                      ▼                      ▼
    ┌────────────────┐    ┌──────────────────┐    ┌────────────────┐
    │ Vector Storage │    │ Metadata Storage │    │  Query Engine  │
    └───────┬────────┘    └─────────┬────────┘    └───────┬────────┘
            │                       │                     │
            ▼                       ▼                     ▼
    ┌────────────────┐    ┌──────────────────┐    ┌────────────────┐
    │ ID Hash Index  │    │ Hash/Bitmap/B+   │    │ Exact or HNSW  │
    └────────────────┘    │ Tree Indexes     │    │ Vector Search  │
                          └──────────────────┘    └────────────────┘
                                    │
                                    ▼
                        ┌──────────────────────┐
                        │ WAL + Segment Files  │
                        └──────────────────────┘
```

---

## 5. Suggested repository structure

```text
vectordb-from-scratch/
├── CMakeLists.txt
├── README.md
├── include/vectordb/
│   ├── database.hpp
│   ├── vector_store.hpp
│   ├── distance.hpp
│   ├── top_k.hpp
│   ├── hash_table.hpp
│   ├── metadata.hpp
│   ├── serializer.hpp
│   ├── wal.hpp
│   ├── bloom_filter.hpp
│   ├── lru_cache.hpp
│   ├── bplus_tree.hpp
│   └── hnsw.hpp
├── src/
│   ├── database.cpp
│   ├── vector_store.cpp
│   ├── distance.cpp
│   ├── metadata.cpp
│   ├── serializer.cpp
│   ├── wal.cpp
│   ├── bloom_filter.cpp
│   ├── lru_cache.cpp
│   ├── bplus_tree.cpp
│   └── hnsw.cpp
├── tests/
│   ├── vector_store_test.cpp
│   ├── distance_test.cpp
│   ├── hash_table_test.cpp
│   ├── persistence_test.cpp
│   ├── recovery_test.cpp
│   ├── metadata_test.cpp
│   └── hnsw_test.cpp
├── benchmarks/
│   ├── exact_search_benchmark.cpp
│   ├── storage_layout_benchmark.cpp
│   └── hnsw_benchmark.cpp
├── tools/
│   └── vectordb_cli.cpp
├── examples/
├── data/
└── notes/
```

---

# Part I — Build a correct in-memory vector database

## Milestone 0: Define the product before coding

### Learn

Read about:

- vectors and embeddings
- nearest-neighbor search
- exact versus approximate search
- cosine similarity, dot product, and Euclidean distance

Useful resources:

- [Google Machine Learning: Embeddings](https://developers.google.com/machine-learning/crash-course/embeddings)
- [Sentence Transformers: Semantic search](https://www.sbert.net/examples/sentence_transformer/applications/semantic-search/README.html)
- [Faiss research overview](https://faiss.ai/)

### Write a short design document

Decide:

- fixed or variable vector dimensions
- allowed numeric type: start with `float`
- whether IDs are integers or strings
- whether duplicate IDs overwrite or fail
- similarity functions supported
- deletion behavior
- error-handling policy

Recommended first decisions:

```text
Dimensions: fixed when the database is created
Vector type: float
ID type: uint64_t
Duplicate insert: return an error
Deletion: mark a tombstone initially
Similarity: cosine, dot product, Euclidean
```

### Deliverable

Create `notes/00-design.md` containing the API, assumptions, and non-goals.

---

## Milestone 1: Vector storage and memory layout

### Goal

Store vectors in memory and retrieve them by physical position.

### Learn

Study:

- dynamic arrays
- contiguous versus scattered memory
- capacity and reallocation
- cache locality
- row-major storage
- object ownership and RAII

Resources:

- [cppreference: std::vector](https://en.cppreference.com/w/cpp/container/vector)
- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf)

### Version A: simple records

```cpp
struct VectorRecord {
    std::uint64_t id;
    std::vector<float> values;
    bool deleted = false;
};

class VectorStore {
public:
    explicit VectorStore(std::size_t dimensions);

    std::size_t append(std::uint64_t id, std::span<const float> values);
    const VectorRecord& at(std::size_t position) const;
    VectorRecord& at(std::size_t position);
    std::size_t size() const noexcept;

private:
    std::size_t dimensions_;
    std::vector<VectorRecord> records_;
};
```

### Version B: flat contiguous storage

After Version A works, store all vector values in one flat array:

```text
[v0 dimension 0 ... v0 dimension d-1]
[v1 dimension 0 ... v1 dimension d-1]
[v2 dimension 0 ... v2 dimension d-1]
```

The starting offset for vector `i` is:

```text
i × dimensions
```

Suggested representation:

```cpp
std::vector<float> values_;
std::vector<std::uint64_t> ids_;
std::vector<bool> deleted_;
```

### Tests

- insert zero vectors
- insert one vector
- reject the wrong dimension
- retrieve the first and last vector
- force multiple `std::vector` reallocations
- mark and read a tombstone

### Benchmark

Compare:

- vector-of-vectors layout
- flat contiguous layout

Measure sequential scans over 10,000, 100,000, and 1,000,000 vectors.

### Completion check

You should be able to explain why the flat representation often scans faster even though both versions contain the same numbers.

---

## Milestone 2: Build an ID hash table

### Goal

Map a user-facing vector ID to its storage position.

```text
vector ID → physical storage position
```

### Learn

Study:

- hash functions
- load factor
- collisions
- open addressing
- separate chaining
- linear probing
- tombstones
- table resizing
- Robin Hood hashing

Resources:

- [Stanford CS166: Hashing](https://web.stanford.edu/class/archive/cs/cs166/cs166.1256/lectures/04/)
- [MIT OpenCourseWare: Hashing](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-fall-2011/resources/lecture-8-hashing-with-chaining/)

### Build order

1. Use `std::unordered_map<uint64_t, size_t>` to make the database work.
2. Write a separate `OpenAddressHashTable`.
3. Replace the standard map with your custom table.
4. Run the same test suite against both implementations.

Suggested API:

```cpp
class IdIndex {
public:
    bool insert(std::uint64_t id, std::size_t position);
    std::optional<std::size_t> find(std::uint64_t id) const;
    bool erase(std::uint64_t id);
    std::size_t size() const noexcept;
    double load_factor() const noexcept;
};
```

### Required behaviors

- collision handling
- resizing when the load factor crosses a threshold
- deleted-slot tombstones
- duplicate detection
- no infinite probing loops

### Tests

- deliberately generate colliding keys
- insert until several resizes occur
- delete and reinsert colliding keys
- search for missing keys
- compare thousands of random operations with `std::unordered_map`

### Reflection question

Why can a hash table have expected `O(1)` lookup but still produce slow operations occasionally?

---

## Milestone 3: Similarity functions

### Goal

Correctly compare vectors.

Implement:

```cpp
enum class Metric {
    cosine,
    dot_product,
    euclidean
};

float dot_product(std::span<const float> a, std::span<const float> b);
float cosine_similarity(std::span<const float> a, std::span<const float> b);
float squared_euclidean(std::span<const float> a, std::span<const float> b);
```

### Learn

Study:

- dot products
- vector magnitude
- normalization
- cosine similarity
- Euclidean distance
- floating-point error
- why squared Euclidean distance can avoid an unnecessary square root

Resources:

- [3Blue1Brown: Dot products and duality](https://www.3blue1brown.com/lessons/dot-products)
- [Floating Point Guide](https://floating-point-gui.de/)

### Important decisions

- Define what happens for a zero vector.
- State whether a higher score or lower score is better for each metric.
- Consider normalizing vectors during insertion for faster cosine search.

### Tests

- identical vectors
- opposite vectors
- orthogonal vectors
- zero vectors
- different dimensions
- large and very small values
- compare against hand-calculated examples

### Benchmark

Measure search with:

- cosine computed from raw vectors
- normalized vectors using only a dot product

---

## Milestone 4: Exact top-k search with a heap

### Goal

Return the `k` nearest vectors without sorting the entire database.

### Learn

Study:

- binary heaps
- min-heaps versus max-heaps
- priority queues
- top-k selection
- partial sorting

Resources:

- [MIT OpenCourseWare: Heaps and heap sort](https://ocw.mit.edu/courses/6-006-introduction-to-algorithms-fall-2011/resources/lecture-4-heaps-and-heap-sort/)
- [cppreference: priority_queue](https://en.cppreference.com/w/cpp/container/priority_queue)

### Algorithm

For each non-deleted vector:

1. Calculate its score.
2. Add it to a heap.
3. If the heap contains more than `k` results, remove the worst result.
4. Return the remaining results in best-first order.

Complexity:

```text
Distance calculations: O(n × d)
Heap maintenance:      O(n log k)
Extra search memory:   O(k)
```

### Suggested result type

```cpp
struct SearchResult {
    std::uint64_t id;
    float score;
};
```

### Tests

- `k = 0`
- `k = 1`
- `k` greater than the database size
- deleted vectors
- ties
- each distance metric
- compare heap output against full-sort output

### Deliverable

At the end of this milestone, Version 0.1 should support:

```text
insert
get
update
delete
search
```

Tag the release:

```bash
git tag v0.1-in-memory-exact
```

---

# Part II — Make it a persistent database

## Milestone 5: Design a binary file format

### Goal

Save the database to disk and load it later.

### Learn

Study:

- binary serialization
- endianness
- fixed-width integer types
- magic numbers
- version fields
- checksums
- pages and offsets
- forward compatibility

Resources:

- [SQLite database file format](https://www.sqlite.org/fileformat.html)
- [cppreference: fixed-width integers](https://en.cppreference.com/w/cpp/types/integer)

### Suggested header

```cpp
struct FileHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t dimensions;
    std::uint64_t record_count;
    std::uint64_t metadata_offset;
};
```

Do not directly dump arbitrary C++ structs to disk without considering padding and byte order. Write each field explicitly.

### Suggested layout

```text
[header]
[vector records]
[metadata records]
[index data]
[checksum]
```

### Tasks

- write a serializer
- write a deserializer
- validate the magic number
- validate dimensions
- reject truncated files
- add a format version
- add checksums for corruption detection

### Tests

- save and load an empty database
- round-trip random vectors
- load a truncated file
- corrupt the magic number
- corrupt a record length
- try an unsupported version

### Completion check

A database saved by one process must load correctly in a fresh process.

---

## Milestone 6: Write-ahead log and crash recovery

### Goal

Recover committed operations after a crash.

### Learn

Study:

- write-ahead logging
- atomicity and durability
- log sequence numbers
- commit records
- replay and idempotence
- checkpoints
- `fsync`

Resources:

- [SQLite: Write-Ahead Logging](https://www.sqlite.org/wal.html)
- [CMU 15-445: Recovery materials](https://15445.courses.cs.cmu.edu/)

### Simple WAL record format

```text
[record length]
[log sequence number]
[operation type]
[payload]
[checksum]
```

Operation types:

```text
INSERT
UPDATE
DELETE
COMMIT
CHECKPOINT
```

### First recovery model

For a single operation:

1. Append the intended change to the WAL.
2. Flush the WAL.
3. Apply the change in memory.
4. Mark the operation committed.
5. Periodically checkpoint the database.

### Crash experiments

Add a debug setting that terminates the process after each step. Restart and verify that the database recovers into a valid state.

Test crashes:

- before WAL append
- during WAL append
- after WAL flush
- after in-memory update
- before commit record
- after commit but before checkpoint

### Reflection question

Why must the log become durable before the main data file is changed?

---

## Milestone 7: Segments, tombstones, and compaction

### Goal

Move from rewriting one large file to immutable segments.

### Learn

Study:

- append-only storage
- immutable sorted files
- memtables
- tombstones
- compaction
- LSM-tree architecture

Resource:

- [The Log-Structured Merge-Tree paper](https://www.cs.umb.edu/~poneil/lsmtree.pdf)

### Architecture

```text
Writes → mutable memtable → immutable segment
                              ↓
                         background merge
```

Suggested files:

```text
data/
├── MANIFEST
├── WAL
├── segment-000001.vec
├── segment-000001.meta
├── segment-000001.idx
└── segment-000002.vec
```

### Tasks

- write new data to a memtable
- flush when a size threshold is reached
- search newest segments first
- let tombstones hide older records
- compact multiple segments into one
- atomically replace the manifest

### Data structure option

Use a skip list as the memtable to learn another probabilistic structure. Start with `std::map` as a reference.

---

# Part III — Add metadata and traditional database indexes

## Milestone 8: Metadata storage and filtering

### Goal

Support queries such as:

```text
SEARCH query_vector TOP 10
WHERE category = "book" AND year >= 2000
```

### Initial metadata type

```cpp
using MetadataValue = std::variant<std::int64_t, double, bool, std::string>;
using Metadata = std::unordered_map<std::string, MetadataValue>;
```

### Learn

Study:

- schemas versus schemaless data
- typed values
- predicates
- inverted indexes
- set intersection
- pre-filtering versus post-filtering

### Equality index

```text
field name → field value → posting list of vector IDs
```

Example:

```text
category
├── book  → [4, 8, 19, 25]
└── movie → [2, 6, 11]
```

Implement sorted posting lists and intersection using two pointers.

### Tests

- missing fields
- wrong value type
- equality filters
- multiple filters
- updates that change indexed metadata
- deletion from posting lists

---

## Milestone 9: Bitmap indexes

### Goal

Efficiently filter low-cardinality fields such as language, category, or active status.

### Learn

Study:

- bit arrays
- bitwise AND, OR, XOR, and NOT
- population count
- dense versus sparse sets
- compressed bitmaps

Example:

```text
category = book:   1 0 1 1 0 0 1 0
language = en:     1 1 1 0 0 1 1 0
AND result:        1 0 1 0 0 0 1 0
```

### Tasks

- implement a dynamic bitset
- set, clear, and read bits
- combine bitmaps
- iterate only over set bits
- compare performance with `std::unordered_set<uint64_t>`

### Stretch goal

Read about Roaring bitmaps and implement a small simplified version.

Resource:

- [Roaring Bitmap format and implementations](https://roaringbitmap.org/)

---

## Milestone 10: B+ tree range index

### Goal

Support numeric filters:

```text
year >= 2000
price BETWEEN 10 AND 50
```

### Learn

Study:

- multiway balanced trees
- fan-out
- database pages
- internal nodes and leaf nodes
- leaf-level linked lists
- node splitting and merging
- point queries and range scans

Resources:

- [CMU 15-445 B+ tree project](https://15445.courses.cs.cmu.edu/fall2025/project2/)
- [CMU B+ tree lecture](https://www.youtube.com/watch?v=scUtG_6M_lU)

### Build stages

1. In-memory B+ tree with insertion and point lookup.
2. Leaf links and range scans.
3. Deletion without rebalancing.
4. Full deletion with borrowing and merging.
5. Fixed-size page representation.
6. Persist pages to disk.

### Invariants to assert

- keys are ordered
- every internal child range is correct
- all leaves remain at the same depth
- non-root nodes respect minimum occupancy
- leaf links form a valid ordered chain

### Testing strategy

After every random insertion or deletion:

- compare results with `std::map`
- validate all invariants
- run every possible range query for small datasets

---

## Milestone 11: Basic query planner

### Goal

Choose whether to filter before or after vector search.

### Learn

Study:

- selectivity
- cardinality
- query costs
- statistics
- execution plans

Two possible plans:

```text
Plan A: metadata filter → exact vector search over candidates
Plan B: vector search → filter returned candidates
```

### First cost model

Track:

- total active vectors
- number of values per metadata field
- posting-list sizes
- estimated filter selectivity

Example rule:

```text
If the filter is expected to match less than 20% of vectors,
pre-filter before exact vector search.
```

Later, replace this fixed rule with measured costs.

### Deliverable

Add an `EXPLAIN` command:

```text
EXPLAIN SEARCH TOP 10 WHERE category = "book"
```

Possible output:

```text
Metadata index: equality posting list
Estimated candidates: 4,821 / 100,000
Selected plan: pre-filter then exact search
```

---

# Part IV — Build approximate nearest-neighbor indexes

## Milestone 12: KD-tree as a learning index

### Goal

Understand spatial partitioning before tackling HNSW.

### Learn

Study:

- recursive partitioning
- median selection
- bounding regions
- backtracking and pruning
- curse of dimensionality

Build a KD-tree for small dimensions such as 2, 4, 8, and 16.

### Experiments

Measure exact-search speed versus KD-tree speed as dimensions increase.

Record when the KD-tree stops being useful. This experiment will make the curse of dimensionality concrete.

---

## Milestone 13: HNSW search-only prototype

### Goal

Understand greedy graph search before implementing full insertion.

### Required reading

- [Original HNSW paper](https://arxiv.org/abs/1603.09320)
- [Published HNSW article](https://pubmed.ncbi.nlm.nih.gov/30602420/)

Read the original paper in sections. Do not try to understand every formula immediately.

### Concepts to learn first

- nearest-neighbor graph
- small-world graph
- greedy routing
- hierarchical layers
- candidate queue
- visited set
- recall versus latency

### Prototype

Construct a small graph manually or connect every node to its `M` exact nearest neighbors. Then implement greedy search over this graph.

This is not yet full HNSW. It isolates the graph-search logic.

### Search structures

Use:

- min-heap for unexplored candidates
- max-heap for current best results
- hash set or generation-mark array for visited nodes
- adjacency lists for neighbors

### Tests

- hand-built graphs with known paths
- disconnected graphs
- duplicate vectors
- search starting from a poor entry point
- compare returned neighbors with exact search

---

## Milestone 14: Full HNSW insertion

### Goal

Build the hierarchy dynamically.

Suggested node representation:

```cpp
struct HnswNode {
    std::uint64_t id;
    std::uint32_t vector_position;
    int max_level;
    std::vector<std::vector<std::uint32_t>> neighbors;
};
```

### Parameters

```text
M               maximum neighbors per node
M0              maximum neighbors at layer zero
efConstruction  candidate breadth during insertion
efSearch        candidate breadth during search
```

### Build order

1. random level generation
2. entry point and maximum-level tracking
3. greedy descent through upper layers
4. layer search with candidate and result heaps
5. bidirectional neighbor connections
6. neighbor pruning
7. deletion marker support
8. index serialization

### Important invariant checks

- every neighbor ID exists
- no self-loop unless deliberately allowed
- neighbor lists stay within their limit
- links are valid for the node's layer
- entry point is valid
- deleted nodes are not returned

### Correctness measurement

HNSW is approximate, so ordinary pass/fail tests are not enough.

Measure recall:

```text
recall@k = relevant exact neighbors found / k
```

For each query:

1. run exact search to obtain ground truth
2. run HNSW search
3. compare the result sets
4. record latency and recall

### Experiments

Vary:

- `M`
- `efConstruction`
- `efSearch`
- dataset size
- vector dimensions

Plot:

- recall versus query latency
- recall versus memory
- build time versus `efConstruction`

---

# Part V — Systems and performance features

## Milestone 15: Bloom filter

### Goal

Avoid checking a disk segment when an ID is definitely absent.

### Learn

Study:

- probabilistic membership queries
- false positives
- false-negative guarantees
- bit arrays
- multiple hash functions
- sizing formulas

Resources:

- [Stanford CS166: Approximate Membership Queries](https://web.stanford.edu/class/archive/cs/cs166/cs166.1256/lectures/14/)
- [Stanford CS166: Cuckoo Filters](https://web.stanford.edu/class/archive/cs/cs166/cs166.1266/lectures/15/)

### Tests

- no inserted item may be reported absent
- measure false-positive rate with random missing IDs
- compare measured and expected false-positive rates
- test different bit-array sizes and hash counts

---

## Milestone 16: LRU cache

### Goal

Cache recently used pages or vectors in `O(1)` average time.

### Learn

Combine:

- hash table for direct lookup
- doubly linked list for recency order

Required operations:

```text
get: O(1)
put: O(1)
evict least-recently-used: O(1)
```

### Tests

- capacity zero
- capacity one
- update an existing item
- repeated reads change recency
- correct eviction order
- randomized comparison with a slow reference cache

### Experiment

Generate workloads with different locality patterns and measure cache hit rate.

---

## Milestone 17: Concurrency

### Goal

Allow concurrent readers and controlled writers.

### Learn

Study:

- data races
- mutexes
- shared mutexes
- lock ordering
- deadlocks
- atomics
- coarse versus fine-grained locking
- thread pools

Resource:

- [cppreference: shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex)

### Build order

1. make the single-threaded database race-free by design
2. protect everything with one mutex
3. change to a shared mutex: shared reads, exclusive writes
4. separate storage, metadata, and index locks
5. define one global lock order
6. add concurrent stress tests

### Tools

Compile with sanitizers:

```bash
-fsanitize=address,undefined
-fsanitize=thread
```

Do not use AddressSanitizer and ThreadSanitizer in the same run.

---

## Milestone 18: Compression and product quantization

### Goal

Reduce vector memory and speed up approximate distance calculations.

### Learn

Study:

- scalar quantization
- k-means clustering
- subvector partitioning
- codebooks
- lookup-table distance calculations
- compression versus recall

Start with simple scalar quantization from `float32` to `int8`. Measure accuracy loss. Then study product quantization.

Resources:

- [Faiss documentation](https://faiss.ai/)
- [Product Quantization for Nearest Neighbor Search](https://lear.inrialpes.fr/pubs/2011/JDS11/jegou_searching_with_quantization.pdf)

---

# Part VI — Build an application around the engine

## Milestone 19: Command-line interface

Create commands:

```text
CREATE dimensions=384 metric=cosine
INSERT id=101 vector=[...] category=book year=1987
GET id=101
DELETE id=101
SEARCH vector=[...] k=10
SEARCH vector=[...] k=10 WHERE category=book
SAVE
LOAD
STATS
EXPLAIN ...
CHECK
```

`CHECK` should validate database and index invariants.

---

## Milestone 20: HTTP server

After the engine works, expose it through a small service.

Possible endpoints:

```text
POST   /collections
POST   /collections/{name}/vectors
GET    /collections/{name}/vectors/{id}
DELETE /collections/{name}/vectors/{id}
POST   /collections/{name}/search
GET    /collections/{name}/stats
```

Keep networking separate from the database core. The engine should remain usable without the server.

---

# 12-week guided schedule

Adjust the pace based on your classes and other work. Finishing the core correctly matters more than finishing every stretch goal.

## Week 1 — Foundations and storage

Read:

- embeddings and vector similarity
- `std::vector`, spans, and memory layout

Build:

- project setup
- `VectorStore`
- dimensionality validation
- initial unit tests

Deliverable:

- insert and retrieve vectors by physical position

## Week 2 — Hash table and CRUD API

Read:

- open addressing
- load factor
- tombstones and resizing

Build:

- reference ID map
- custom hash table
- `insert`, `get`, `update`, and `delete`

Deliverable:

- randomized equivalence tests against standard containers

## Week 3 — Similarity and exact search

Read:

- dot product, cosine similarity, and Euclidean distance
- heaps and top-k selection

Build:

- all three metrics
- exact scan
- top-k heap

Deliverable:

- Version 0.1 in-memory vector database

## Week 4 — Persistence

Read:

- binary formats
- endianness
- checksums

Build:

- serializer and deserializer
- file validation

Deliverable:

- save/load round-trip and corruption tests

## Week 5 — WAL and recovery

Read:

- logging, commit records, checkpoints, and `fsync`

Build:

- WAL writer
- replay logic
- crash injection

Deliverable:

- recovery test suite

## Week 6 — Metadata and inverted indexes

Read:

- posting lists
- typed predicates
- set intersection

Build:

- metadata storage
- equality filters
- sorted posting lists

Deliverable:

- filtered exact vector search

## Week 7 — Bitmaps and B+ trees

Read:

- bitmap indexes
- B+ tree nodes, splits, and range scans

Build:

- dynamic bitset
- first B+ tree version

Deliverable:

- equality and range metadata filters

## Week 8 — Query planning and segments

Read:

- selectivity and basic cost estimation
- immutable segments and compaction

Build:

- query statistics
- `EXPLAIN`
- segment flush and tombstones

Deliverable:

- database with multiple persistent segments

## Week 9 — KD-tree experiments

Read:

- spatial partitioning and the curse of dimensionality

Build:

- KD-tree
- exactness tests

Deliverable:

- written benchmark showing where KD-trees degrade

## Week 10 — HNSW search prototype

Read:

- original HNSW paper through the search procedure

Build:

- graph representation
- candidate and result heaps
- greedy graph search

Deliverable:

- search on a manually or exactly constructed neighbor graph

## Week 11 — HNSW insertion

Read:

- layer selection
- insertion traversal
- neighbor selection and pruning

Build:

- dynamic insertion
- full hierarchy
- configurable `M`, `efConstruction`, and `efSearch`

Deliverable:

- recall and latency benchmark against exact search

## Week 12 — Cache, Bloom filter, concurrency, and polish

Read:

- Bloom filters
- LRU caches
- shared locking

Build:

- Bloom filter per segment
- page/vector cache
- basic concurrent reading
- CLI and final documentation

Deliverable:

- Version 1.0 demonstration

---

# Testing plan

## Unit tests

Test each data structure separately:

- hash-table probing and resizing
- heap order
- bitmap operations
- B+ tree invariants
- Bloom-filter guarantees
- LRU eviction
- HNSW neighbor constraints

## Property-based and randomized tests

Generate random operation sequences and compare with a trusted reference implementation.

Example:

```text
repeat 100,000 times:
    randomly choose insert/get/update/delete
    run on custom database
    run on reference database
    compare visible state
```

## Persistence tests

- round-trip save/load
- corrupted headers
- truncated records
- invalid checksums
- old format versions

## Recovery tests

Terminate the process at controlled points and verify recovery.

## Approximate-search tests

Track:

- recall@1
- recall@10
- p50 query latency
- p95 query latency
- p99 query latency
- index construction time
- index memory per vector

---

# Benchmark datasets

Start with generated vectors so that debugging is easy.

### Synthetic datasets

1. uniformly random vectors
2. clustered vectors
3. duplicate vectors
4. vectors concentrated around a few directions
5. adversarial insertion orders

### Public ANN datasets

After the system is stable, explore:

- [ANN Benchmarks](https://ann-benchmarks.com/)
- SIFT1M
- GloVe vectors
- Fashion-MNIST embeddings

Always keep a small dataset that exact search can process to produce ground truth.

---

# Benchmark questions to answer

Do not only report raw times. Explain the results.

1. When does flat storage outperform vector-of-vectors storage?
2. How does hash-table load factor affect lookup latency?
3. When is a heap faster than sorting all search results?
4. How much does pre-normalizing vectors help cosine search?
5. At what dimension does the KD-tree lose its advantage?
6. How does `efSearch` affect HNSW recall and latency?
7. How much memory does each HNSW edge consume?
8. When does metadata pre-filtering beat post-filtering?
9. What workload produces a high LRU hit rate?
10. How does Bloom-filter size affect disk checks and false positives?

---

# Version roadmap

## Version 0.1 — Exact in-memory engine

- fixed dimensions
- CRUD operations
- custom ID hash table
- cosine, dot-product, and Euclidean metrics
- exact top-k search

## Version 0.2 — Persistent engine

- binary file format
- save/load
- checksums
- WAL and recovery

## Version 0.3 — Filtered search

- typed metadata
- equality index
- posting-list intersection
- bitmap index
- range index

## Version 0.4 — Segmented storage

- memtable
- immutable segments
- tombstones
- compaction
- Bloom filters

## Version 0.5 — Approximate search

- KD-tree experiment
- HNSW insertion and querying
- recall benchmark suite

## Version 1.0 — Complete learning database

- query planner
- HNSW persistence
- LRU cache
- basic concurrency
- CLI or HTTP service
- benchmarks and design report

---

# Definition of done

The core project is complete when:

- CRUD operations behave correctly.
- Exact search matches a full-sort reference implementation.
- Saved data loads in a fresh process.
- Committed WAL operations survive injected crashes.
- Metadata filters return correct IDs.
- B+ tree range queries match `std::map` results.
- HNSW reaches a measured recall target on a documented dataset.
- Benchmarks include latency, memory, and correctness measurements.
- Sanitizers report no known memory errors or data races in tested paths.
- The README explains major design choices and tradeoffs.

---

# Questions you should be able to answer afterward

- Why are contiguous arrays good for vector scans?
- Why does a database need both a storage position and a stable external ID?
- Why is top-k search usually implemented with a heap?
- Why do B+ trees have high fan-out?
- Why can Bloom filters return false positives but not false negatives?
- Why does a write-ahead log need to be flushed before data pages?
- Why do tombstones exist in hash tables and LSM-style storage?
- Why do KD-trees struggle in high-dimensional spaces?
- How does HNSW trade exactness for speed?
- How do `M`, `efConstruction`, and `efSearch` change HNSW behavior?
- Why can metadata selectivity change the best query plan?
- What makes a cache workload-dependent?
- Which structures are memory-bound and which are CPU-bound?

---

# First coding session

Complete only these tasks:

1. Create the repository and CMake project.
2. Add GoogleTest.
3. Define `VectorRecord` and `VectorStore`.
4. Enforce fixed dimensions.
5. Write five storage tests.
6. Add a tiny CLI that inserts and prints one vector.
7. Write `notes/01-memory-layout.md` in your own words.

Do **not** begin search or HNSW during the first session.

A good first commit:

```text
feat: add fixed-dimension in-memory vector storage
```

---

# Optional stretch goals

Choose these only after the main system works:

- memory-mapped segment files
- SIMD distance calculations
- scalar and product quantization
- sparse vectors
- hybrid keyword and vector search
- replication
- consistent-hash sharding
- snapshot isolation
- transactions spanning several operations
- distributed top-k result merging
- background index rebuilding
- graph compaction
- a visual HNSW explorer

---

# Final presentation idea

Demonstrate the database with a small semantic-search application:

1. split several public-domain books into passages
2. generate embeddings using an external model
3. insert vectors and metadata into your database
4. search passages by meaning
5. filter by book, author, or year
6. compare exact and HNSW results
7. display latency and recall

Keep embedding generation outside the storage engine. Your project is the database, not the embedding model.

---

# Progress checklist

```text
[ ] Milestone 0: design document
[ ] Milestone 1: vector storage
[ ] Milestone 2: custom ID hash table
[ ] Milestone 3: distance metrics
[ ] Milestone 4: exact top-k search
[ ] Milestone 5: binary persistence
[ ] Milestone 6: WAL and recovery
[ ] Milestone 7: segments and compaction
[ ] Milestone 8: metadata and equality filters
[ ] Milestone 9: bitmap index
[ ] Milestone 10: B+ tree range index
[ ] Milestone 11: query planner
[ ] Milestone 12: KD-tree experiment
[ ] Milestone 13: HNSW search prototype
[ ] Milestone 14: HNSW insertion
[ ] Milestone 15: Bloom filters
[ ] Milestone 16: LRU cache
[ ] Milestone 17: concurrency
[ ] Milestone 18: quantization
[ ] Milestone 19: CLI
[ ] Milestone 20: HTTP service
```

The best next step is **Milestone 0**, followed by the first coding session at the end of this README.
