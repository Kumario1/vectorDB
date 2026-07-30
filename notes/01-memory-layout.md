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
(bullet list of what you implemented and tested)