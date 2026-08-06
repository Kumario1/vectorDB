#include "vectordb/crash.hpp"

#include <cstdlib>

namespace vectordb {


static CrashPoint crash_point = CrashPoint::None;

void set_crash_point(CrashPoint p) {
    crash_point = p;
}

void maybe_crash(CrashPoint p) {
    if (crash_point == p) {
        std::abort();
    }
}

} // namespace vectordb