// Segment format sandbox — write a few rows (incl. tombstone), read back, print.

#include "vectordb/segment.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

int main() {
    std::filesystem::create_directories("data");
    const std::string path = "data/segment_sandbox.vec";

    {
        vectordb::SegmentWriter writer(path);
        if (writer.open(2, vectordb::Metric::cosine) != vectordb::Status::ok) {
            std::cerr << "writer open failed\n";
            return 1;
        }

        vectordb::SegmentRow a;
        a.id = 101;
        a.values = {0.1f, 0.2f};
        if (writer.append(a) != vectordb::Status::ok) {
            std::cerr << "append a failed\n";
            return 1;
        }

        vectordb::SegmentRow b;
        b.id = 55;
        b.values = {1.0f, 0.0f};
        if (writer.append(b) != vectordb::Status::ok) {
            std::cerr << "append b failed\n";
            return 1;
        }

        vectordb::SegmentRow tomb;
        tomb.id = 99;
        tomb.is_deleted = true;
        if (writer.append(tomb) != vectordb::Status::ok) {
            std::cerr << "append tombstone failed\n";
            return 1;
        }

        if (writer.finish() != vectordb::Status::ok) {
            std::cerr << "finish failed\n";
            return 1;
        }
    }

    vectordb::SegmentReader reader(path);
    if (reader.open() != vectordb::Status::ok) {
        std::cerr << "reader open failed\n";
        return 1;
    }

    std::vector<vectordb::SegmentRow> rows;
    if (reader.read_all(rows) != vectordb::Status::ok) {
        std::cerr << "read_all failed\n";
        return 1;
    }

    std::cout << "read " << rows.size() << " rows from " << path << "\n";
    for (const auto& row : rows) {
        std::cout << "id=" << row.id << " deleted=" << row.is_deleted;
        if (!row.is_deleted) {
            std::cout << " values=[";
            for (std::size_t i = 0; i < row.values.size(); ++i) {
                if (i) {
                    std::cout << ", ";
                }
                std::cout << row.values[i];
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }
    return 0;
}
