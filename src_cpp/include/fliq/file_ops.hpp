#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::file_ops — High-performance file I/O engine
// Translated from the Rust runtime/file_ops.rs with RAII semantics
// ─────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fliq {

// ── Constants ────────────────────────────────────────────────────────
constexpr uint64_t MAX_READ_SIZE  = 10ULL * 1024 * 1024;  // 10 MB
constexpr size_t   MAX_WRITE_SIZE = 10ULL * 1024 * 1024;  // 10 MB

// ── Structured output types (mirrors Rust serde structs) ─────────────

struct TextFilePayload {
    std::string file_path;
    std::string content;
    size_t      num_lines   = 0;
    size_t      start_line  = 0;
    size_t      total_lines = 0;
};

struct ReadFileOutput {
    std::string     kind;  // "text"
    TextFilePayload file;
};

struct StructuredPatchHunk {
    size_t                   old_start = 0;
    size_t                   old_lines = 0;
    size_t                   new_start = 0;
    size_t                   new_lines = 0;
    std::vector<std::string> lines;
};

struct WriteFileOutput {
    std::string                       kind;  // "create" | "update"
    std::string                       file_path;
    std::string                       content;
    std::vector<StructuredPatchHunk>  structured_patch;
    std::optional<std::string>        original_file;
};

struct EditFileOutput {
    std::string                       file_path;
    std::string                       old_string;
    std::string                       new_string;
    std::string                       original_file;
    std::vector<StructuredPatchHunk>  structured_patch;
    bool                              user_modified = false;
    bool                              replace_all   = false;
};

struct GlobSearchOutput {
    uint64_t                  duration_ms = 0;
    size_t                    num_files   = 0;
    std::vector<std::string>  filenames;
    bool                      truncated   = false;
};

struct GrepSearchInput {
    std::string            pattern;
    std::optional<std::string> path;
    std::optional<std::string> glob;
    std::optional<std::string> output_mode;
    std::optional<size_t>  before;
    std::optional<size_t>  after;
    std::optional<size_t>  context;
    std::optional<bool>    line_numbers;
    std::optional<bool>    case_insensitive;
    std::optional<std::string> file_type;
    std::optional<size_t>  head_limit;
    std::optional<size_t>  offset;
};

struct GrepSearchOutput {
    std::optional<std::string> mode;
    size_t                     num_files   = 0;
    std::vector<std::string>   filenames;
    std::optional<std::string> content;
    std::optional<size_t>      num_lines;
    std::optional<size_t>      num_matches;
    std::optional<size_t>      applied_limit;
    std::optional<size_t>      applied_offset;
};

// ── Core API ─────────────────────────────────────────────────────────

/// Check if a file is binary (contains NUL bytes in the first 8 KiB).
bool is_binary_file(const std::string& path);

/// Validate that `resolved` stays within `workspace_root`.
bool validate_workspace_boundary(const std::string& resolved,
                                 const std::string& workspace_root);

/// Read a file with optional offset/limit (line-based pagination).
ReadFileOutput read_file(const std::string& path,
                         std::optional<size_t> offset = std::nullopt,
                         std::optional<size_t> limit  = std::nullopt);

/// Write content to a file (creates parent dirs as needed).
WriteFileOutput write_file(const std::string& path, const std::string& content);

/// Edit a file by replacing occurrences of old_string with new_string.
EditFileOutput edit_file(const std::string& path,
                         const std::string& old_string,
                         const std::string& new_string,
                         bool replace_all = false);

/// Glob-pattern search starting from an optional base directory.
GlobSearchOutput glob_search(const std::string& pattern,
                             std::optional<std::string> base_path = std::nullopt);

/// Regex-based grep across files under a directory tree.
GrepSearchOutput grep_search(const GrepSearchInput& input);

/// Read with workspace boundary enforcement.
ReadFileOutput read_file_in_workspace(const std::string& path,
                                      std::optional<size_t> offset,
                                      std::optional<size_t> limit,
                                      const std::string& workspace_root);

/// Write with workspace boundary enforcement.
WriteFileOutput write_file_in_workspace(const std::string& path,
                                        const std::string& content,
                                        const std::string& workspace_root);

}  // namespace fliq
