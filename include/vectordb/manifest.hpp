#pragma once

#include "vectordb/database.hpp"
#include <string>
#include <vector>



//--------------------------------
// Format (text, oldest → newest):
//   VECMAN01\n
//   <version>\n          # 1
//   <count>\n
//   <filename>\n         # relative names, one per line
//
// replace(dir): write MANIFEST.tmp → fflush+fsync → rename over MANIFEST.
// load(dir): read MANIFEST only; missing file = empty list; ignore MANIFEST.tmp.
//--------------------------------

namespace vectordb {

    class Manifest {
        public:
            Status load(const std::string& dir);
            Status replace(const std::string& dir) const;

            void add(std::string filename);
            const std::vector<std::string>& segments() const noexcept;
            void clear();
        private:
            std::vector<std::string> segments_; // oldest -> newest
    };

} // namespace vectordb