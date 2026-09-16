#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

// The C++ standard library already gives us a monotonic clock and a portable
// sleep, so no platform-specific code is needed for timing.  Keeping these
// helpers here means the rest of the application never talks to <chrono>
// directly and the units are always explicit.

namespace mdview {

inline std::uint64_t monotonicMillis() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

inline void sleepMillis(std::uint64_t millis) {
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

}  // namespace mdview
