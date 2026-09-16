#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace mdview::test {

/// A unique, self-cleaning scratch directory.  Every test that touches the
/// filesystem gets its own, so tests can run in any order and never collide.
class TempDirectory {
public:
    explicit TempDirectory(const std::string& tag) {
        static std::atomic<int> counter{0};
        std::error_code error;
        path_ = std::filesystem::temp_directory_path(error) /
                ("mdview-test-" + tag + "-" + std::to_string(counter.fetch_add(1)));
        std::filesystem::remove_all(path_, error);
        error.clear();
        std::filesystem::create_directories(path_, error);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    const std::filesystem::path& path() const { return path_; }

    void write(const std::string& name, const std::string& content) const {
        const std::filesystem::path target = path_ / name;
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream stream(target, std::ios::binary | std::ios::trunc);
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void makeDirectory(const std::string& name) const {
        std::error_code error;
        std::filesystem::create_directories(path_ / name, error);
    }

private:
    std::filesystem::path path_;
};

}  // namespace mdview::test
