# VectorDB From Scratch

A small vector database in C++20, built milestone by milestone to learn the data structures and systems ideas behind real vector stores.

This is a **learning project**, not a production competitor to Faiss/Milvus/etc.

## Current status

| Milestone | Status |
|-----------|--------|
| 0 — Design document | Done (`notes/00-design.md`) |
| 1a — `VectorStore` (record-per-vector) | Done |
| 1b — `FlatVectorStore` (contiguous floats) | Done |
| 1c — A vs B scan benchmark | Next |
| 2+ — ID hash index, metrics, search, … | Not started |

Full roadmap: [`README_VectorDB_From_Scratch.md`](README_VectorDB_From_Scratch.md)

## What works today

- Fixed dimensions at construction
- Append / read by **physical position**
- Dimension validation
- Tombstone flag (soft delete)
- Two storage layouts:
  - **Version A** — `VectorStore`: `vector<VectorRecord>` (simple)
  - **Version B** — `FlatVectorStore`: SoA with one contiguous `float` buffer (cache-friendlier scans)

ID → position lookup, similarity metrics, and search come in later milestones.

## Requirements

- CMake ≥ 3.20
- C++20 compiler (Apple Clang / GCC / MSVC)
- Network on first configure (GoogleTest via FetchContent)

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## CLI

Tiny demo that inserts and prints one vector:

```bash
./build/vectordb_cli
```

## Layout

```text
include/vectordb/   public headers
src/                implementations
tests/              GoogleTest suites
tools/              CLI and utilities
notes/              design + learning notes
benchmarks/         (coming) A vs B scans
```

## Design notes

- [Milestone 0 design](notes/00-design.md) — API assumptions and non-goals
- [Memory layout notes](notes/01-memory-layout.md) — Version A learning log

## License

Personal / educational use unless otherwise stated.
