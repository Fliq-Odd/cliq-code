#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::directory_walker — Fast recursive directory traversal
// Translated from Rust walkdir usage patterns in file_ops.rs
// ─────────────────────────────────────────────────────────────────────

#include <functional>
#include <string>
#include <vector>

namespace fliq {

/// Recursively collect all regular file paths under `root_dir`.
std::vector<std::string> collect_files(const std::string& root_dir);

/// Recursively collect files matching an extension filter.
std::vector<std::string> collect_files_with_extension(
    const std::string& root_dir,
    const std::string& extension);

/// Walk and invoke a callback on each file path. Return false to stop.
void walk_directory(const std::string& root_dir,
                    const std::function<bool(const std::string&)>& visitor);

/// Get basic directory stats: total files, total size in bytes.
struct DirectoryStats {
    size_t   total_files     = 0;
    uint64_t total_size_bytes = 0;
};
DirectoryStats get_directory_stats(const std::string& root_dir);

}  // namespace fliq
