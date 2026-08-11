#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>

#include "vectordb/database.hpp"



//------------------------------ recommended format of segment file ------------------------------
// [ magic           8 ]   'V','E','C','S','E','G','0','1'
// [ version         u32 ] 1
// [ dimensions      u32 ] fixed for all rows in this segment
// [ record_count    u64 ] number of physical rows (live + tombstone)
// [ metric          u32 ] 0=cosine, 1=dot, 2=euclidean (same as .vdb)

// [ ids             u64 × record_count ]
// [ deleted         u8  × record_count ]   0 = live, 1 = tombstone
// [ floats          f32 × record_count × dimensions ]

// [ checksum        u32 ] byte-sum of everything BEFORE checksum (same as .vdb/WAL)
//----------------------------------------------------------------------------


namespace vectordb {

    inline constexpr char kSegmentMagic[8] = {'V','E','C','S','E','G','0','1'};
    inline constexpr std::uint32_t kSegmentVersion = 1;

    //each segement file contains a list of rows
    struct SegmentRow {
        std::uint64_t id = 0;
        bool is_deleted = false;
        std::vector<float> values; //size == diemnsions when !deleted; empty ok when deleted
    };

    //writer for segment file
    class SegmentWriter {
    public:
        explicit SegmentWriter(std::string path);
        ~SegmentWriter();

        SegmentWriter(const SegmentWriter&) = delete;
        SegmentWriter& operator=(const SegmentWriter&) = delete;

        Status open(std::size_t dimensions, Metric metric);
        Status append(const SegmentRow& row);
        Status finish();

    private:
        std::string path_;
        FILE* file_ = nullptr;
        std::vector<SegmentRow> rows_;
        std::size_t dimensions_ = 0;
        Metric metric_ = Metric::cosine;
    };

    //reader for segment file
    class SegmentReader {
    public:
        explicit SegmentReader(std::string path);
        ~SegmentReader();

        SegmentReader(const SegmentReader&) = delete;
        SegmentReader& operator=(const SegmentReader&) = delete;

        Status open();
        Status read_all(std::vector<SegmentRow>& rows);

    private:
        std::string path_;
        FILE* file_ = nullptr;
    };
}