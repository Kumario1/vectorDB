# Milestone 1 — Memory layout (Version A)

## What problem does this structure solve?
- we need a place to store the vectorstores, so we keep a dynamically growing array taht keeps the position, dimentions, and we can get it by the position etc.
- we need RAM sotrage for embedding so search can scan them. rows are addresed by positon; id is the position, easy to find.

## What are its invariants?
- we have cna just keep a flat stored

## Expected time complexity?
(append, at, full scan)
append - O(1)
at - O(pos)
full - O(size) , where N is the size

## Memory cost?
(hint: each VectorRecord owns its own std::vector<float>)
N * (id + deleted + head vector (std::vector<fload>), allocator overhead)

## What inputs / situations perform poorly?
appending once its full
full scan many times to find the row we are looking for
mayn small allocations and reallocations while growing records.

## Why is it useful in this database?
simple, and testable. 

## What I built
- Version A `VectorStore` (record-per-vector) with tests and CLI
- Version B `FlatVectorStore` (contiguous float buffer) with tests
- `benchmarks/storage_layout_benchmark.cpp` comparing sequential scans

## Benchmark: A vs B sequential scan

Release (`-O3`), median of 5 timed runs after one warm-up. Checksums kept so the loops are not optimized away. Four independent accumulators to avoid a single FP add chain hiding memory effects.

| dims | n | A (s) | B (s) | A/B |
|------|------|----------|----------|--------|
| 4 | 10k | 0.000023 | 0.000022 | 1.01 |
| 4 | 100k | 0.000238 | 0.000221 | 1.08 |
| 4 | 1M | 0.00241 | 0.00233 | 1.03 |
| 128 | 10k | 0.000298 | 0.000281 | 1.06 |
| 128 | 100k | 0.00366 | 0.00277 | **1.32** |
| 128 | 1M | 0.0286 | 0.0303 | 0.94 |

### Takeaways
- Flat layout can win: at **128-d × 100k**, A was ~32% slower (pointer chasing / worse locality).
- At **1M × 128** (~512 MB of floats), both are near DRAM bandwidth (~17–18 GB/s), so layout barely matters.
- At **d = 4**, working sets often stay cache-friendly and the allocator may place A's blocks nearly in a row, so the gap stays small.
- Earlier “same time” results were misleading: unoptimized builds drown layout in loop overhead, and discarding scan checksums lets `-O3` delete the work.