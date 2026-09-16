#pragma once

#include <string_view>

// Platform identity and standard-stream queries.
//
// This is deliberately tiny: only things that genuinely cannot be answered
// portably live here.  Timing, environment variables, the filesystem and the
// Terminal/FileWatcher factories all have portable homes elsewhere
// (utils/time.hpp, utils/env.hpp, <filesystem>, terminal/ and filesystem/).

namespace mdview::platform {

/// "windows", "linux" or "macos".
std::string_view name();

bool stdoutIsTerminal();
bool stdinIsTerminal();

}  // namespace mdview::platform
