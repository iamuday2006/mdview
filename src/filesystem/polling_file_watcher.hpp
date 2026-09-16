#pragma once

#include "filesystem/file_watcher.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

// The portable FileWatcher: it remembers each watched path's
// last-write-time and size and compares them on a throttled interval.
//
// It is intentionally the default on every platform.  It uses nothing but
// <filesystem>, so it compiles and behaves identically under MSVC, GCC and
// Clang, and it cannot miss an event by mis-using a platform API.  The cost is
// latency: a change is noticed up to one poll interval late (400 ms by
// default), which is imperceptible for "my editor saved the file" reloads.

namespace mdview::filesystem {

class PollingFileWatcher final : public FileWatcher {
public:
    explicit PollingFileWatcher(std::uint64_t intervalMillis = 400);

    void watch(const std::filesystem::path& path) override;
    void unwatch(const std::filesystem::path& path) override;
    bool changed() override;
    std::vector<std::filesystem::path> takeChanged() override;

    void setIntervalMillis(std::uint64_t intervalMillis) { intervalMillis_ = intervalMillis; }

private:
    struct Entry {
        std::filesystem::path path;
        std::filesystem::file_time_type modified{};
        std::uintmax_t size = 0;
        bool exists = false;
    };

    static Entry snapshot(const std::filesystem::path& path);
    void poll();

    std::vector<Entry> entries_;
    std::vector<std::filesystem::path> pendingChanges_;
    std::uint64_t intervalMillis_;
    std::uint64_t lastPollMillis_ = 0;
    bool polledOnce_ = false;
};

}  // namespace mdview::filesystem
