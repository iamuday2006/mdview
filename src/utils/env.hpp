#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

// std::getenv is part of the C runtime on every platform we target, so
// environment access does not need to live in the platform layer.

namespace mdview::env {

inline std::optional<std::string> get(std::string_view name) {
    const std::string key(name);
    if (const char* value = std::getenv(key.c_str())) {
        return std::string(value);
    }
    return std::nullopt;
}

inline int getInt(std::string_view name, int fallback) {
    const auto raw = get(name);
    if (!raw || raw->empty()) return fallback;
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(*raw, &consumed);
        return consumed == 0 ? fallback : value;
    } catch (...) {
        return fallback;
    }
}

}  // namespace mdview::env
