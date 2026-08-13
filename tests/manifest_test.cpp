#include "vectordb/manifest.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using vectordb::Manifest;
using vectordb::Status;

class ManifestTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_manifest_test";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    std::string dir() const { return dir_.string(); }

    fs::path dir_;
};

TEST_F(ManifestTest, StartsEmpty) {
    Manifest m;
    EXPECT_TRUE(m.segments().empty());
}

TEST_F(ManifestTest, AddAndClear) {
    Manifest m;
    m.add("segment-000001.vec");
    m.add("segment-000002.vec");
    ASSERT_EQ(m.segments().size(), 2u);
    EXPECT_EQ(m.segments()[0], "segment-000001.vec");
    EXPECT_EQ(m.segments()[1], "segment-000002.vec");
    m.clear();
    EXPECT_TRUE(m.segments().empty());
}

TEST_F(ManifestTest, LoadMissingManifestIsEmptyOk) {
    Manifest m;
    m.add("should-be-cleared.vec");
    ASSERT_EQ(m.load(dir()), Status::ok);
    EXPECT_TRUE(m.segments().empty());
}

TEST_F(ManifestTest, ReplaceThenLoadRoundTrip) {
    {
        Manifest m;
        m.add("segment-000001.vec");
        m.add("segment-000002.vec");
        ASSERT_EQ(m.replace(dir()), Status::ok);
    }

    EXPECT_TRUE(fs::exists(dir_ / "MANIFEST"));
    EXPECT_FALSE(fs::exists(dir_ / "MANIFEST.tmp"));

    Manifest loaded;
    ASSERT_EQ(loaded.load(dir()), Status::ok);
    ASSERT_EQ(loaded.segments().size(), 2u);
    EXPECT_EQ(loaded.segments()[0], "segment-000001.vec");
    EXPECT_EQ(loaded.segments()[1], "segment-000002.vec");
}

TEST_F(ManifestTest, ReplaceOverwritesPreviousManifest) {
    Manifest first;
    first.add("segment-000001.vec");
    ASSERT_EQ(first.replace(dir()), Status::ok);

    Manifest second;
    second.add("segment-000003.vec");
    second.add("segment-000004.vec");
    ASSERT_EQ(second.replace(dir()), Status::ok);

    Manifest loaded;
    ASSERT_EQ(loaded.load(dir()), Status::ok);
    ASSERT_EQ(loaded.segments().size(), 2u);
    EXPECT_EQ(loaded.segments()[0], "segment-000003.vec");
    EXPECT_EQ(loaded.segments()[1], "segment-000004.vec");
}

TEST_F(ManifestTest, ReplaceEmptyList) {
    Manifest m;
    ASSERT_EQ(m.replace(dir()), Status::ok);

    Manifest loaded;
    ASSERT_EQ(loaded.load(dir()), Status::ok);
    EXPECT_TRUE(loaded.segments().empty());
}

TEST_F(ManifestTest, IgnoresLeftoverTmpOnLoad) {
    {
        Manifest m;
        m.add("segment-000001.vec");
        ASSERT_EQ(m.replace(dir()), Status::ok);
    }
    {
        std::ofstream junk(dir_ / "MANIFEST.tmp");
        junk << "VECMAN01\n1\n1\nshould-not-load.vec\n";
    }

    Manifest loaded;
    ASSERT_EQ(loaded.load(dir()), Status::ok);
    ASSERT_EQ(loaded.segments().size(), 1u);
    EXPECT_EQ(loaded.segments()[0], "segment-000001.vec");
}

TEST_F(ManifestTest, RejectsBadMagic) {
    {
        std::ofstream f(dir_ / "MANIFEST");
        f << "BADMAGIC\n1\n0\n";
    }
    Manifest m;
    EXPECT_EQ(m.load(dir()), Status::invalid_argument);
}

TEST_F(ManifestTest, RejectsCountMismatch) {
    {
        std::ofstream f(dir_ / "MANIFEST");
        f << "VECMAN01\n1\n2\nsegment-000001.vec\n";
    }
    Manifest m;
    EXPECT_EQ(m.load(dir()), Status::invalid_argument);
}

TEST_F(ManifestTest, RejectsEmptyFilenameOnReplace) {
    Manifest m;
    m.add("segment-000001.vec");
    m.add("");
    EXPECT_EQ(m.replace(dir()), Status::invalid_argument);
    EXPECT_FALSE(fs::exists(dir_ / "MANIFEST"));
}
