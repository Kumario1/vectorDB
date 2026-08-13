#include "vectordb/manifest.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace vectordb{
    constexpr const char* kMagic = "VECMAN01";
    constexpr std::uint32_t kVersion = 1;


    void Manifest::add(std::string filename) {
        segments_.push_back(filename);
    }

    const std::vector<std::string>& Manifest::segments() const noexcept {
        return segments_;
    }

    void Manifest::clear() {
        segments_.clear();
    }


    Status Manifest::load(const std::string& dir) {
        std::string path = dir + "/MANIFEST";

        std::ifstream file(path);
        if (!file.is_open()) {
            //empty DB 
            segments_.clear();
            return Status::ok;
        }
        
        //read magic
        std::string magic;
        std::getline(file, magic);
        if (magic != kMagic) {
            return Status::invalid_argument;
        }

        //read version (read as integer)
        std::string version_str;
        std::getline(file, version_str);
        std::uint32_t version = std::stoi(version_str);
        if (version != kVersion) {
            return Status::invalid_argument;
        }

        std::string count_str;
        std::getline(file, count_str);
        std::size_t count = std::stoul(count_str);


        //read exactly count lines, if not equal to count, return invalid_argument
        std::vector<std::string> filenames;
        std::string filename;
        while (std::getline(file, filename)) {
            if (filename.empty()) {
                return Status::invalid_argument;
            }
            filenames.push_back(filename);
        }

        if (filenames.size() != count) {
            return Status::invalid_argument;
        }
        
        segments_ = filenames;
        return Status::ok;
    }

    Status Manifest::replace(const std::string& dir) const {
        std::string tmp_path = dir + "/MANIFEST.tmp";

        FILE* f = std::fopen(tmp_path.c_str(), "wb");

        if(!f) {
            return Status::invalid_argument;
        }



        // FILE* so fflush + fsync(fileno) match WalWriter durability.
        std::fprintf(f, "%s\n", kMagic);
        std::fprintf(f, "%u\n", kVersion);
        std::fprintf(f, "%zu\n", segments_.size());
        for (const auto& filename : segments_) {
            if (filename.empty()) {
                std::fclose(f);
                return Status::invalid_argument;
            }
            std::fprintf(f, "%s\n", filename.c_str());
        }

        if (std::fflush(f) != 0) {
            std::fclose(f);
            return Status::invalid_argument;
        }
        if (fsync(fileno(f)) != 0) {
            std::fclose(f);
            return Status::invalid_argument;
        }
        std::fclose(f);

        // Atomic publish: crash leaves either old MANIFEST or new one.
        if (std::rename(tmp_path.c_str(), (dir + "/MANIFEST").c_str()) != 0) {
            return Status::invalid_argument;
        }
        return Status::ok;
    }

}