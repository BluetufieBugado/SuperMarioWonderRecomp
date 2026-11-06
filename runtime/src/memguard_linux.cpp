// Memory guard (Linux), as memguard_mac.cpp: the process's memory is checked every 250 ms; past the limit the process
// ends at once, so a leak can never push the machine into swap until it freezes. The limit is NX_MEM_LIMIT_GB
// (default 8, as on macOS and Windows); the footprint is logged every 30 s so growth shows before it matters.
//
// It reads resident pages from /proc/self/statm: the kernel's counters, no page-table walk. Not smaps_rollup, which
// walks every page-table entry of the multi-GB guest heap under the mm lock and stalls the game's page faults and
// mprotects behind it (memguard_android.cpp, docs/nvn/android-perf/PHASE4.md). GPU memory on a discrete card is not
// in it; on an integrated GPU the driver's mappings mostly are.
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <nx/runtime.h>

#include "platform.h"

namespace nx::plat {

namespace {

uint64_t resident_bytes() {
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long long size = 0, resident = 0;
    if (std::fscanf(f, "%llu %llu", &size, &resident) != 2) resident = 0;
    std::fclose(f);
    return uint64_t(resident) * uint64_t(sysconf(_SC_PAGESIZE));
}

}  // namespace

void start_memory_guard() {
    const char* e = std::getenv("NX_MEM_LIMIT_GB");
    double gb = e ? std::atof(e) : 8.0;
    if (gb <= 0) gb = 8.0;
    const uint64_t limit = uint64_t(gb * double(1ull << 30));
    std::thread([limit, gb] {
        uint64_t ticks = 0, peak = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            uint64_t f = resident_bytes();
            if (f > peak) peak = f;
            if (++ticks % 120 == 0)
                log("memory: %.2f GB resident (peak %.2f GB, limit %.1f GB)", double(f) / double(1ull << 30),
                    double(peak) / double(1ull << 30), gb);
            if (f > limit) {
                char msg[160];
                std::snprintf(msg, sizeof msg, "memory: %.2f GB is over the %.1f GB limit (NX_MEM_LIMIT_GB): stopping",
                              double(f) / double(1ull << 30), gb);
                log("%s", msg);
                std::fprintf(stderr, "%s\n", msg);
                _exit(3);
            }
        }
    }).detach();
}

uint64_t memguard_overlap_ns(uint64_t, uint64_t) { return 0; }

}  // namespace nx::plat
