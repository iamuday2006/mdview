#pragma once

#include <filesystem>
#include <memory>
#include <vector>

// File watching, abstracted.
//
// The application only ever sees this interface.  It therefore never depends on
// ReadDirectoryChangesW, inotify or kqueue, and it cannot be broken by an OS
// that implements watching differently.  The factory lives in
// src/platform/<os>/ so a native backend can replace the polling one without
// touching a single call site.

namespace mdview::filesystem {

class FileWatcher {
public:
    virtual ~FileWatcher();

    /// Starts tracking a file (or directory) for modifications.
    virtual void watch(const std::filesystem::path& path) = 0;
    virtual void unwatch(const std::filesystem::path& path) = 0;

    /// True when something changed since the previous call.  Implementations
    /// may rate-limit how often they hit the filesystem.
    virtual bool changed() = 0;

    /// The paths behind the most recent positive changed(), consumed once.
    virtual std::vector<std::filesystem::path> takeChanged() = 0;
};

/// Creates the watcher for the platform this binary was built for.
std::unique_ptr<FileWatcher> createFileWatcher();

}  // namespace mdview::filesystem
