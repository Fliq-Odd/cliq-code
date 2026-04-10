// ─────────────────────────────────────────────────────────────────────
// fliq::directory_walker — Implementation
// High-performance recursive directory traversal using std::filesystem
// ─────────────────────────────────────────────────────────────────────

#include "fliq/directory_walker.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace fliq {

std::vector<std::string> collect_files(const std::string& root_dir) {
    std::vector<std::string> files;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root_dir, ec)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

std::vector<std::string> collect_files_with_extension(
    const std::string& root_dir,
    const std::string& extension) {

    std::vector<std::string> files;
    std::string dot_ext = extension;
    if (!dot_ext.empty() && dot_ext[0] != '.') {
        dot_ext = "." + dot_ext;
    }

    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root_dir, ec)) {
        if (entry.is_regular_file() &&
            entry.path().extension().string() == dot_ext) {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

void walk_directory(const std::string& root_dir,
                    const std::function<bool(const std::string&)>& visitor) {
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root_dir, ec)) {
        if (entry.is_regular_file()) {
            if (!visitor(entry.path().string())) {
                return;  // visitor signaled stop
            }
        }
    }
}

DirectoryStats get_directory_stats(const std::string& root_dir) {
    DirectoryStats stats;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root_dir, ec)) {
        if (entry.is_regular_file()) {
            stats.total_files++;
            stats.total_size_bytes += entry.file_size(ec);
        }
    }
    return stats;
}

}  // namespace fliq
