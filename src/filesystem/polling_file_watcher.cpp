#include "filesystem/polling_file_watcher.hpp"

#include "utils/time.hpp"

#include <algorithm>

namespace mdview::filesystem {

namespace fs = std::filesystem;

FileWatcher::~FileWatcher() = default;

PollingFileWatcher::PollingFileWatcher(std::uint64_t intervalMillis)
    : intervalMillis_(intervalMillis) {}

PollingFileWatcher::Entry PollingFileWatcher::snapshot(const fs::path& path) {
    Entry entry;
    entry.path = path;

    std::error_code error;
    const auto status = fs::status(path, error);
    if (error || !fs::exists(status)) {
        entry.exists = false;
        return entry;
    }

    entry.exists = true;
    entry.modified = fs::last_write_time(path, error);
    if (error) entry.modified = fs::file_time_type{};
    error.clear();
    entry.size = fs::file_size(path, error);
    if (error) entry.size = 0;
    return entry;
}

void PollingFileWatcher::watch(const fs::path& path) {
    // Normalise so that the same file watched twice is tracked once.
    std::error_code error;
    fs::path normalised = fs::weakly_canonical(path, error);
    if (error) normalised = path;
    if (normalised.empty()) normalised = path;

    const auto existing = std::find_if(entries_.begin(), entries_.end(),
                                       [&](const Entry& entry) { return entry.path == normalised; });
    if (existing != entries_.end()) {
        existing->modified = snapshot(normalised).modified;
        return;
    }
    entries_.push_back(snapshot(normalised));
}

void PollingFileWatcher::unwatch(const fs::path& path) {
    std::error_code error;
    fs::path normalised = fs::weakly_canonical(path, error);
    if (error) normalised = path;

    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const Entry& entry) { return entry.path == normalised; }),
                   entries_.end());
    pendingChanges_.erase(
        std::remove(pendingChanges_.begin(), pendingChanges_.end(), normalised),
        pendingChanges_.end());
}

void PollingFileWatcher::poll() {
    for (Entry& entry : entries_) {
        const Entry current = snapshot(entry.path);
        const bool differs = current.exists != entry.exists || current.size != entry.size ||
                             current.modified != entry.modified;
        if (!differs) continue;

        entry.exists = current.exists;
        entry.size = current.size;
        entry.modified = current.modified;

        if (std::find(pendingChanges_.begin(), pendingChanges_.end(), entry.path) ==
            pendingChanges_.end()) {
            pendingChanges_.push_back(entry.path);
        }
    }
}

bool PollingFileWatcher::changed() {
    const std::uint64_t now = monotonicMillis();
    if (polledOnce_ && now - lastPollMillis_ < intervalMillis_) {
        return !pendingChanges_.empty();
    }
    lastPollMillis_ = now;
    polledOnce_ = true;
    poll();
    return !pendingChanges_.empty();
}

std::vector<fs::path> PollingFileWatcher::takeChanged() {
    std::vector<fs::path> result;
    result.swap(pendingChanges_);
    return result;
}

}  // namespace mdview::filesystem
