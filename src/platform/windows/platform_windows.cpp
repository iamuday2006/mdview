#include "platform/platform.hpp"

#include "filesystem/file_watcher.hpp"
#include "filesystem/polling_file_watcher.hpp"

#include <io.h>

#include <cstdio>
#include <memory>

namespace mdview::platform {

std::string_view name() { return "windows"; }

bool stdoutIsTerminal() { return _isatty(_fileno(stdout)) != 0; }

bool stdinIsTerminal() { return _isatty(_fileno(stdin)) != 0; }

}  // namespace mdview::platform

namespace mdview::filesystem {

std::unique_ptr<FileWatcher> createFileWatcher() {
    // Seam for a native backend.  ReadDirectoryChangesW with overlapped I/O
    // could be dropped in here, which would make reloads instant instead of
    // bounded by the poll interval.  The polling watcher is the default so that
    // every platform behaves identically; see docs/ARCHITECTURE.md.
    return std::make_unique<PollingFileWatcher>();
}

}  // namespace mdview::filesystem
