#include "vectordb/crash.hpp"
#include "vectordb/database.hpp"
#include "vectordb/wal.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t kCrashTestId = 42;
constexpr float kCrashTestVec[] = {1.0f, 2.0f};
constexpr float kCrashCheckpointA[] = {1.0f, 2.0f};
constexpr float kCrashCheckpointB[] = {5.0f, 6.0f};

void child_crashing_insert(const std::string& wal_path, vectordb::CrashPoint point) {
    vectordb::set_crash_point(point);
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    if (db.enable_wal(wal_path) != vectordb::Status::ok) {
        _exit(10);
    }
    if (db.insert(kCrashTestId, kCrashTestVec) != vectordb::Status::ok) {
        _exit(11);
    }
    _exit(12);  // crash point did not fire
}

void child_crashing_checkpoint(const std::string& wal_path,
                               const std::string& snap_path,
                               vectordb::CrashPoint point) {
    vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    if (db.enable_wal(wal_path) != vectordb::Status::ok) {
        _exit(20);
    }
    if (db.insert(10, kCrashCheckpointA) != vectordb::Status::ok) {
        _exit(21);
    }
    if (db.insert(20, kCrashCheckpointB) != vectordb::Status::ok) {
        _exit(22);
    }
    vectordb::set_crash_point(point);
    if (db.checkpoint(snap_path) != vectordb::Status::ok) {
        _exit(23);
    }
    _exit(24);  // crash point did not fire
}

void expect_child_aborted(pid_t pid) {
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status)) << "status=" << status;
    EXPECT_EQ(WTERMSIG(status), SIGABRT);
}

void fork_crashing_insert(const std::string& wal_path, vectordb::CrashPoint point) {
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        child_crashing_insert(wal_path, point);
    }
    expect_child_aborted(pid);
}

void fork_crashing_checkpoint(const std::string& wal_path,
                              const std::string& snap_path,
                              vectordb::CrashPoint point) {
    const pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        child_crashing_checkpoint(wal_path, snap_path, point);
    }
    expect_child_aborted(pid);
}

}  // namespace

class WalTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "vectordb_wal_test";
        fs::create_directories(dir_);
        path_ = (dir_ / "test.wal").string();
        fs::remove(path_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    std::vector<vectordb::WalRecord> read_all() {
        std::vector<vectordb::WalRecord> out;
        vectordb::WalReader reader(path_);
        EXPECT_EQ(reader.open(), vectordb::Status::ok);
        for (;;) {
            vectordb::WalRecord rec;
            bool have = false;
            EXPECT_EQ(reader.read_next(rec, have), vectordb::Status::ok);
            if (!have) {
                break;
            }
            out.push_back(std::move(rec));
        }
        return out;
    }

    fs::path dir_;
    std::string path_;
};

TEST_F(WalTest, AppendTwoInsertsRoundTrip) {
    {
        vectordb::WalWriter writer(path_);
        ASSERT_EQ(writer.open(), vectordb::Status::ok);

        vectordb::WalRecord a;
        a.op = vectordb::WalOp::Insert;
        a.id = 42;
        a.dimensions = 2;
        a.values = {1.0f, 2.0f};
        ASSERT_EQ(writer.append(a), vectordb::Status::ok);
        EXPECT_EQ(a.lsn, 1u);

        vectordb::WalRecord b;
        b.op = vectordb::WalOp::Insert;
        b.id = 99;
        b.dimensions = 2;
        b.values = {3.0f, 4.0f};
        ASSERT_EQ(writer.append(b), vectordb::Status::ok);
        EXPECT_EQ(b.lsn, 2u);

        ASSERT_EQ(writer.flush(), vectordb::Status::ok);
    }

    vectordb::WalReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);

    vectordb::WalRecord got;
    bool have = false;

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    ASSERT_TRUE(have);
    EXPECT_EQ(got.lsn, 1u);
    EXPECT_EQ(got.op, vectordb::WalOp::Insert);
    EXPECT_EQ(got.id, 42u);
    EXPECT_EQ(got.dimensions, 2u);
    ASSERT_EQ(got.values.size(), 2u);
    EXPECT_EQ(got.values[0], 1.0f);
    EXPECT_EQ(got.values[1], 2.0f);

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    ASSERT_TRUE(have);
    EXPECT_EQ(got.lsn, 2u);
    EXPECT_EQ(got.id, 99u);
    ASSERT_EQ(got.values.size(), 2u);
    EXPECT_EQ(got.values[0], 3.0f);
    EXPECT_EQ(got.values[1], 4.0f);

    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    EXPECT_FALSE(have);
}

TEST_F(WalTest, RejectsDimensionMismatch) {
    vectordb::WalWriter writer(path_);
    ASSERT_EQ(writer.open(), vectordb::Status::ok);

    vectordb::WalRecord bad;
    bad.op = vectordb::WalOp::Insert;
    bad.id = 1;
    bad.dimensions = 2;
    bad.values = {1.0f};  // wrong size
    EXPECT_EQ(writer.append(bad), vectordb::Status::dimension_mismatch);
}

TEST_F(WalTest, AppendCheckpointRoundTrip) {
    {
        vectordb::WalWriter writer(path_);
        ASSERT_EQ(writer.open(), vectordb::Status::ok);

        vectordb::WalRecord cp;
        cp.op = vectordb::WalOp::Checkpoint;
        cp.checkpoint_lsn = 7;
        ASSERT_EQ(writer.append(cp), vectordb::Status::ok);
        EXPECT_EQ(cp.lsn, 1u);
        ASSERT_EQ(writer.flush(), vectordb::Status::ok);
    }

    const auto records = read_all();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].op, vectordb::WalOp::Checkpoint);
    EXPECT_EQ(records[0].lsn, 1u);
    EXPECT_EQ(records[0].checkpoint_lsn, 7u);
}

TEST_F(WalTest, MissingFileOpenFails) {
    const auto missing = (dir_ / "nope.wal").string();
    vectordb::WalReader reader(missing);
    EXPECT_EQ(reader.open(), vectordb::Status::invalid_argument);
}

TEST_F(WalTest, EmptyFileEof) {
    {
        vectordb::WalWriter writer(path_);
        ASSERT_EQ(writer.open(), vectordb::Status::ok);
        ASSERT_EQ(writer.flush(), vectordb::Status::ok);
    }
    vectordb::WalReader reader(path_);
    ASSERT_EQ(reader.open(), vectordb::Status::ok);
    vectordb::WalRecord got;
    bool have = true;
    ASSERT_EQ(reader.read_next(got, have), vectordb::Status::ok);
    EXPECT_FALSE(have);
}

TEST_F(WalTest, VectorDBEnableWalLogsInsertUpdateDelete) {
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);

        const float a[] = {1.0f, 2.0f};
        const float b[] = {9.0f, 8.0f};
        ASSERT_EQ(db.insert(10, a), vectordb::Status::ok);
        ASSERT_EQ(db.update(10, b), vectordb::Status::ok);
        ASSERT_EQ(db.remove(10), vectordb::Status::ok);
        EXPECT_EQ(db.size(), 0u);
        EXPECT_FALSE(db.get(10).has_value());
    }  // destroy DB so WAL file is closed

    const auto records = read_all();
    ASSERT_EQ(records.size(), 3u);

    EXPECT_EQ(records[0].op, vectordb::WalOp::Insert);
    EXPECT_EQ(records[0].lsn, 1u);
    EXPECT_EQ(records[0].id, 10u);
    ASSERT_EQ(records[0].values.size(), 2u);
    EXPECT_EQ(records[0].values[0], 1.0f);
    EXPECT_EQ(records[0].values[1], 2.0f);

    EXPECT_EQ(records[1].op, vectordb::WalOp::Update);
    EXPECT_EQ(records[1].lsn, 2u);
    EXPECT_EQ(records[1].id, 10u);
    ASSERT_EQ(records[1].values.size(), 2u);
    EXPECT_EQ(records[1].values[0], 9.0f);
    EXPECT_EQ(records[1].values[1], 8.0f);

    EXPECT_EQ(records[2].op, vectordb::WalOp::Delete);
    EXPECT_EQ(records[2].lsn, 3u);
    EXPECT_EQ(records[2].id, 10u);
}

TEST_F(WalTest, DuplicateInsertDoesNotWriteWal) {
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);
        const float v[] = {1.0f, 0.0f};
        ASSERT_EQ(db.insert(1, v), vectordb::Status::ok);
        EXPECT_EQ(db.insert(1, v), vectordb::Status::duplicate_id);
    }

    const auto records = read_all();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].op, vectordb::WalOp::Insert);
    EXPECT_EQ(records[0].id, 1u);
}

TEST_F(WalTest, UpdateMissingDoesNotWriteWal) {
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);
        const float v[] = {1.0f, 0.0f};
        EXPECT_EQ(db.update(999, v), vectordb::Status::not_found);
    }

    const auto records = read_all();
    EXPECT_TRUE(records.empty());
}

TEST_F(WalTest, RemoveMissingDoesNotWriteWal) {
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);
        EXPECT_EQ(db.remove(999), vectordb::Status::not_found);
    }

    const auto records = read_all();
    EXPECT_TRUE(records.empty());
}

TEST_F(WalTest, OpenReplaysWalWithoutSnapshot) {
    const float a[] = {1.0f, 0.0f};
    const float b[] = {0.0f, 1.0f};
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);
        ASSERT_EQ(db.insert(1, a), vectordb::Status::ok);
        ASSERT_EQ(db.insert(2, b), vectordb::Status::ok);
        ASSERT_EQ(db.remove(1), vectordb::Status::ok);
    }

    {
        vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(loaded.open("", path_), vectordb::Status::ok);
        EXPECT_EQ(loaded.size(), 1u);
        EXPECT_FALSE(loaded.get(1).has_value());
        ASSERT_TRUE(loaded.get(2).has_value());
        EXPECT_EQ(loaded.get(2)->data()[1], 1.0f);

        // New mutation continues LSN after replay (max was 3 → next is 4).
        const float c[] = {3.0f, 4.0f};
        ASSERT_EQ(loaded.insert(3, c), vectordb::Status::ok);
    }

    const auto records = read_all();
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[3].lsn, 4u);
    EXPECT_EQ(records[3].op, vectordb::WalOp::Insert);
    EXPECT_EQ(records[3].id, 3u);
}

TEST_F(WalTest, CheckpointThenOpenUsesSnapshotOnly) {
    const std::string snap = (dir_ / "snap.vdb").string();
    const float a[] = {1.0f, 2.0f};
    const float b[] = {5.0f, 6.0f};
    {
        vectordb::VectorDB db(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
        ASSERT_EQ(db.enable_wal(path_), vectordb::Status::ok);
        ASSERT_EQ(db.insert(10, a), vectordb::Status::ok);
        ASSERT_EQ(db.insert(20, b), vectordb::Status::ok);
        ASSERT_EQ(db.checkpoint(snap), vectordb::Status::ok);
        ASSERT_EQ(db.insert(30, a), vectordb::Status::ok);
    }

    // WAL after checkpoint should only hold post-checkpoint ops.
    {
        const auto records = read_all();
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].op, vectordb::WalOp::Insert);
        EXPECT_EQ(records[0].lsn, 1u);
        EXPECT_EQ(records[0].id, 30u);
    }

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open(snap, path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 3u);
    EXPECT_TRUE(loaded.get(10).has_value());
    EXPECT_TRUE(loaded.get(20).has_value());
    EXPECT_TRUE(loaded.get(30).has_value());
}

TEST_F(WalTest, CrashAfterWalFlushRecoversInsertOnReopen) {
    fork_crashing_insert(path_, vectordb::CrashPoint::AfterWalFlush);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open("", path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 1u);
    ASSERT_TRUE(loaded.get(kCrashTestId).has_value());
    EXPECT_EQ(loaded.get(kCrashTestId)->data()[0], 1.0f);
    EXPECT_EQ(loaded.get(kCrashTestId)->data()[1], 2.0f);
}

TEST_F(WalTest, CrashBeforeWalAppendDoesNotRecoverInsert) {
    fork_crashing_insert(path_, vectordb::CrashPoint::BeforeWalAppend);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open("", path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 0u);
    EXPECT_FALSE(loaded.get(kCrashTestId).has_value());
}

// Policy: on this platform, fwrite bytes often reach the kernel even before
// fflush (abort does not reliably drop the stdio buffer). Process-crash
// recovery therefore sees the insert. Guaranteed durable commit point remains
// AfterWalFlush (fflush + fsync).
TEST_F(WalTest, CrashAfterWalAppendBeforeFlushRecoversInsertOnReopen) {
    fork_crashing_insert(path_, vectordb::CrashPoint::AfterWalAppendBeforeFlush);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open("", path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 1u);
    ASSERT_TRUE(loaded.get(kCrashTestId).has_value());
}

TEST_F(WalTest, CrashAfterMemoryApplyRecoversInsertOnReopen) {
    fork_crashing_insert(path_, vectordb::CrashPoint::AfterMemoryApply);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open("", path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 1u);
    ASSERT_TRUE(loaded.get(kCrashTestId).has_value());
    EXPECT_EQ(loaded.get(kCrashTestId)->data()[0], 1.0f);
    EXPECT_EQ(loaded.get(kCrashTestId)->data()[1], 2.0f);
}

TEST_F(WalTest, CrashAfterCheckpointSnapshotRecoversFromSnapAndWal) {
    const std::string snap = (dir_ / "crash_after_snap.vdb").string();
    fork_crashing_checkpoint(path_, snap, vectordb::CrashPoint::AfterCheckpointSnapshot);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open(snap, path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 2u);
    EXPECT_TRUE(loaded.get(10).has_value());
    EXPECT_TRUE(loaded.get(20).has_value());
}

TEST_F(WalTest, CrashAfterCheckpointBeforeTruncateDoesNotDoubleApply) {
    const std::string snap = (dir_ / "crash_before_trunc.vdb").string();
    fork_crashing_checkpoint(
        path_, snap, vectordb::CrashPoint::AfterCheckpointBeforeTruncateWal);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open(snap, path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 2u);
    EXPECT_TRUE(loaded.get(10).has_value());
    EXPECT_TRUE(loaded.get(20).has_value());
}

// Abort after CHECKPOINT is flushed but before truncate so the record is
// still on disk — proves VectorDB::checkpoint wrote it with the right LSN.
TEST_F(WalTest, CheckpointWritesCheckpointRecordBeforeTruncate) {
    const std::string snap = (dir_ / "checkpoint_record.vdb").string();
    fork_crashing_checkpoint(
        path_, snap, vectordb::CrashPoint::AfterCheckpointBeforeTruncateWal);

    const auto records = read_all();
    ASSERT_EQ(records.size(), 3u);

    EXPECT_EQ(records[0].op, vectordb::WalOp::Insert);
    EXPECT_EQ(records[0].lsn, 1u);
    EXPECT_EQ(records[0].id, 10u);

    EXPECT_EQ(records[1].op, vectordb::WalOp::Insert);
    EXPECT_EQ(records[1].lsn, 2u);
    EXPECT_EQ(records[1].id, 20u);

    EXPECT_EQ(records[2].op, vectordb::WalOp::Checkpoint);
    EXPECT_EQ(records[2].lsn, 3u);
    EXPECT_EQ(records[2].checkpoint_lsn, 2u);

    vectordb::VectorDB loaded(2, vectordb::Metric::cosine, vectordb::StorageMode::legacy);
    ASSERT_EQ(loaded.open(snap, path_), vectordb::Status::ok);
    EXPECT_EQ(loaded.size(), 2u);
    EXPECT_TRUE(loaded.get(10).has_value());
    EXPECT_TRUE(loaded.get(20).has_value());
}
