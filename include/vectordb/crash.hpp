#pragma once

namespace vectordb {

enum class CrashPoint{
    None,
    BeforeWalAppend,
    AfterWalAppendBeforeFlush,
    AfterWalFlush,
    AfterMemoryApply,
    AfterCheckpointSnapshot,
    AfterCheckpointBeforeTruncateWal,
};


void set_crash_point(CrashPoint p);
void maybe_crash(CrashPoint p);

} // namespace vectordb