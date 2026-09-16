#include "platform/platform.hpp"

#include "filesystem/file_watcher.hpp"
#include "filesystem/polling_file_watcher.hpp"

#include <unistd.h>

#include <memory>

// The Terminal implementation lives in src/platform/posix/terminal_posix.cpp,
// which is shared with Linux.  This file owns the macOS-specific surface.

namespace mdview::platform {

std::string_view name() { return "macos"; }

bool stdoutIsTerminal() { return ::isatty(STDOUT_FILENO) != 0; }

bool stdinIsTerminal() { return ::isatty(STDIN_FILENO) != 0; }

}  // namespace mdview::platform

namespace mdview::filesystem {

std::unique_ptr<FileWatcher> createFileWatcher() {
    // Seam for a native backend: a kqueue watcher (EVFILT_VNODE with
    // NOTE_WRITE on the tracked file descriptor) plugs in here and would
    // remove the polling latency.  Polling is used for now so that all three
    // platforms share one verified code path; see docs/ARCHITECTURE.md.
    return std::make_unique<PollingFileWatcher>();
}

}  // namespace mdview::filesystem
