// ─────────────────────────────────────────────────────────────────────
// fliq::file_ops — Implementation
// Translated from Rust crates/runtime/src/file_ops.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/file_ops.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace fliq {

// ── Helpers ──────────────────────────────────────────────────────────

static std::string normalize_path(const std::string& path) {
    fs::path p(path);
    if (p.is_absolute()) {
        return fs::canonical(p).string();
    }
    return fs::canonical(fs::current_path() / p).string();
}

static std::string normalize_path_allow_missing(const std::string& path) {
    fs::path p(path);
    fs::path candidate = p.is_absolute() ? p : fs::current_path() / p;

    std::error_code ec;
    auto canonical = fs::canonical(candidate, ec);
    if (!ec) return canonical.string();

    // Parent might exist even if the file doesn't yet
    if (candidate.has_parent_path()) {
        auto parent = fs::canonical(candidate.parent_path(), ec);
        if (!ec && candidate.has_filename()) {
            return (parent / candidate.filename()).string();
        }
    }
    return candidate.string();
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static size_t count_lines(const std::string& text) {
    if (text.empty()) return 0;
    size_t count = 1;
    for (char c : text) {
        if (c == '\n') ++count;
    }
    // Don't count trailing newline as an extra line
    if (!text.empty() && text.back() == '\n') --count;
    return count;
}

static std::vector<StructuredPatchHunk> make_patch(const std::string& original,
                                                    const std::string& updated) {
    std::vector<std::string> lines;
    for (const auto& l : split_lines(original)) {
        lines.push_back("-" + l);
    }
    for (const auto& l : split_lines(updated)) {
        lines.push_back("+" + l);
    }

    StructuredPatchHunk hunk;
    hunk.old_start = 1;
    hunk.old_lines = count_lines(original);
    hunk.new_start = 1;
    hunk.new_lines = count_lines(updated);
    hunk.lines     = std::move(lines);

    return {hunk};
}

static std::string read_entire_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void ensure_parent_dirs(const std::string& path) {
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
}

static std::string replace_first(const std::string& src,
                                  const std::string& from,
                                  const std::string& to) {
    auto pos = src.find(from);
    if (pos == std::string::npos) return src;
    std::string result = src;
    result.replace(pos, from.size(), to);
    return result;
}

static std::string replace_all_occurrences(const std::string& src,
                                            const std::string& from,
                                            const std::string& to) {
    if (from.empty()) return src;
    std::string result = src;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

// ── Public API ───────────────────────────────────────────────────────

bool is_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open file: " + path);
    }
    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    auto bytes_read = file.gcount();
    return std::memchr(buffer, '\0', static_cast<size_t>(bytes_read)) != nullptr;
}

bool validate_workspace_boundary(const std::string& resolved,
                                 const std::string& workspace_root) {
    std::string root = workspace_root;
    if (!root.empty() && root.back() != '/' && root.back() != '\\') {
        root += '/';
    }
    std::string norm_resolved = resolved;
    // Normalize to forward slashes for comparison
    std::replace(norm_resolved.begin(), norm_resolved.end(), '\\', '/');
    std::replace(root.begin(), root.end(), '\\', '/');

    return norm_resolved.rfind(root, 0) == 0 ||
           norm_resolved == workspace_root;
}

ReadFileOutput read_file(const std::string& path,
                         std::optional<size_t> offset,
                         std::optional<size_t> limit) {
    std::string abs_path = normalize_path(path);

    // Size check
    auto file_size = fs::file_size(abs_path);
    if (file_size > MAX_READ_SIZE) {
        throw std::runtime_error(
            "file is too large (" + std::to_string(file_size) +
            " bytes, max " + std::to_string(MAX_READ_SIZE) + " bytes)");
    }

    // Binary check
    if (is_binary_file(abs_path)) {
        throw std::runtime_error("file appears to be binary");
    }

    std::string content = read_entire_file(abs_path);
    auto lines = split_lines(content);

    size_t start_index = std::min(offset.value_or(0), lines.size());
    size_t end_index   = limit.has_value()
        ? std::min(start_index + limit.value(), lines.size())
        : lines.size();

    std::string selected;
    for (size_t i = start_index; i < end_index; ++i) {
        if (i > start_index) selected += '\n';
        selected += lines[i];
    }

    ReadFileOutput out;
    out.kind                = "text";
    out.file.file_path      = abs_path;
    out.file.content        = std::move(selected);
    out.file.num_lines      = end_index - start_index;
    out.file.start_line     = start_index + 1;
    out.file.total_lines    = lines.size();
    return out;
}

WriteFileOutput write_file(const std::string& path, const std::string& content) {
    if (content.size() > MAX_WRITE_SIZE) {
        throw std::runtime_error(
            "content is too large (" + std::to_string(content.size()) +
            " bytes, max " + std::to_string(MAX_WRITE_SIZE) + " bytes)");
    }

    std::string abs_path = normalize_path_allow_missing(path);

    // Try to read existing file
    std::optional<std::string> original_file;
    {
        std::ifstream existing(abs_path);
        if (existing.is_open()) {
            std::ostringstream ss;
            ss << existing.rdbuf();
            original_file = ss.str();
        }
    }

    ensure_parent_dirs(abs_path);
    {
        std::ofstream out(abs_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            throw std::runtime_error("cannot write file: " + abs_path);
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    WriteFileOutput out;
    out.kind             = original_file.has_value() ? "update" : "create";
    out.file_path        = abs_path;
    out.content          = content;
    out.structured_patch = make_patch(
        original_file.value_or(""), content);
    out.original_file    = original_file;
    return out;
}

EditFileOutput edit_file(const std::string& path,
                         const std::string& old_string,
                         const std::string& new_string,
                         bool replace_all) {
    std::string abs_path = normalize_path(path);
    std::string original = read_entire_file(abs_path);

    if (old_string == new_string) {
        throw std::runtime_error("old_string and new_string must differ");
    }
    if (original.find(old_string) == std::string::npos) {
        throw std::runtime_error("old_string not found in file");
    }

    std::string updated = replace_all
        ? replace_all_occurrences(original, old_string, new_string)
        : replace_first(original, old_string, new_string);

    {
        std::ofstream out(abs_path, std::ios::binary | std::ios::trunc);
        out.write(updated.data(), static_cast<std::streamsize>(updated.size()));
    }

    EditFileOutput out;
    out.file_path        = abs_path;
    out.old_string       = old_string;
    out.new_string       = new_string;
    out.original_file    = original;
    out.structured_patch = make_patch(original, updated);
    out.user_modified    = false;
    out.replace_all      = replace_all;
    return out;
}

GlobSearchOutput glob_search(const std::string& pattern,
                             std::optional<std::string> base_path) {
    auto started = std::chrono::steady_clock::now();

    fs::path base = base_path.has_value()
        ? fs::path(normalize_path(base_path.value()))
        : fs::current_path();

    // Simple glob: iterate recursively & match filename against pattern
    std::vector<fs::path> matches;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(base, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        // Simple wildcard matching: *.ext or exact name
        bool matched = false;
        if (pattern.find('*') != std::string::npos) {
            // Extract extension pattern like "*.rs"
            std::string ext_pattern = pattern;
            if (ext_pattern.rfind("**/", 0) == 0) {
                ext_pattern = ext_pattern.substr(3);
            }
            if (ext_pattern.rfind("*.", 0) == 0) {
                std::string ext = ext_pattern.substr(1);  // ".rs"
                matched = filename.size() >= ext.size() &&
                          filename.substr(filename.size() - ext.size()) == ext;
            } else {
                matched = true;  // fallback: include everything
            }
        } else {
            matched = (filename == pattern);
        }
        if (matched) {
            matches.push_back(entry.path());
        }
    }

    // Sort by modification time (most recent first)
    std::sort(matches.begin(), matches.end(),
              [](const fs::path& a, const fs::path& b) {
                  std::error_code ec1, ec2;
                  return fs::last_write_time(a, ec1) > fs::last_write_time(b, ec2);
              });

    bool truncated = matches.size() > 100;
    if (truncated) matches.resize(100);

    auto elapsed = std::chrono::steady_clock::now() - started;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    GlobSearchOutput out;
    out.duration_ms = static_cast<uint64_t>(ms);
    out.num_files   = matches.size();
    out.truncated   = truncated;
    for (auto& m : matches) {
        out.filenames.push_back(m.string());
    }
    return out;
}

GrepSearchOutput grep_search(const GrepSearchInput& input) {
    fs::path base = input.path.has_value()
        ? fs::path(normalize_path(input.path.value()))
        : fs::current_path();

    auto flags = std::regex_constants::ECMAScript;
    if (input.case_insensitive.value_or(false)) {
        flags |= std::regex_constants::icase;
    }
    std::regex regex(input.pattern, flags);

    std::string output_mode = input.output_mode.value_or("files_with_matches");
    size_t ctx = input.context.value_or(0);
    std::vector<std::string> filenames;
    std::vector<std::string> content_lines;
    size_t total_matches = 0;

    // Collect files
    std::vector<fs::path> all_files;
    std::error_code ec;
    if (fs::is_regular_file(base)) {
        all_files.push_back(base);
    } else {
        for (auto& entry : fs::recursive_directory_iterator(base, ec)) {
            if (!entry.is_regular_file()) continue;
            // Extension filter
            if (input.file_type.has_value()) {
                auto ext = entry.path().extension().string();
                if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                if (ext != input.file_type.value()) continue;
            }
            all_files.push_back(entry.path());
        }
    }

    for (auto& file_path : all_files) {
        std::string file_contents;
        try {
            file_contents = read_entire_file(file_path.string());
        } catch (...) {
            continue;
        }

        if (output_mode == "count") {
            auto begin = std::sregex_iterator(file_contents.begin(),
                                               file_contents.end(), regex);
            auto end = std::sregex_iterator();
            size_t count = std::distance(begin, end);
            if (count > 0) {
                filenames.push_back(file_path.string());
                total_matches += count;
            }
            continue;
        }

        auto lines = split_lines(file_contents);
        std::vector<size_t> matched_indices;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (std::regex_search(lines[i], regex)) {
                ++total_matches;
                matched_indices.push_back(i);
            }
        }

        if (matched_indices.empty()) continue;
        filenames.push_back(file_path.string());

        if (output_mode == "content") {
            size_t before = input.before.value_or(ctx);
            size_t after  = input.after.value_or(ctx);
            for (size_t idx : matched_indices) {
                size_t start = idx >= before ? idx - before : 0;
                size_t stop  = std::min(idx + after + 1, lines.size());
                for (size_t cur = start; cur < stop; ++cur) {
                    std::string prefix;
                    if (input.line_numbers.value_or(true)) {
                        prefix = file_path.string() + ":" +
                                 std::to_string(cur + 1) + ":";
                    } else {
                        prefix = file_path.string() + ":";
                    }
                    content_lines.push_back(prefix + lines[cur]);
                }
            }
        }
    }

    // Apply limit/offset
    size_t off = input.offset.value_or(0);
    size_t lim = input.head_limit.value_or(250);

    GrepSearchOutput out;
    out.mode      = output_mode;
    out.num_files = filenames.size();

    if (output_mode == "content") {
        if (off < content_lines.size()) {
            auto start_it = content_lines.begin() +
                            static_cast<ptrdiff_t>(off);
            auto end_it   = (lim > 0 && off + lim < content_lines.size())
                ? content_lines.begin() + static_cast<ptrdiff_t>(off + lim)
                : content_lines.end();
            std::string joined;
            for (auto it = start_it; it != end_it; ++it) {
                if (!joined.empty()) joined += '\n';
                joined += *it;
            }
            out.content    = joined;
            out.num_lines  = static_cast<size_t>(end_it - start_it);
        }
        out.filenames = filenames;
    } else if (output_mode == "count") {
        out.filenames   = filenames;
        out.num_matches = total_matches;
    } else {
        // files_with_matches
        if (off < filenames.size()) {
            auto start_it = filenames.begin() +
                            static_cast<ptrdiff_t>(off);
            auto end_it = (lim > 0 && off + lim < filenames.size())
                ? filenames.begin() + static_cast<ptrdiff_t>(off + lim)
                : filenames.end();
            out.filenames.assign(start_it, end_it);
        }
        out.num_files = out.filenames.size();
    }

    if (off > 0) out.applied_offset = off;
    return out;
}

ReadFileOutput read_file_in_workspace(const std::string& path,
                                      std::optional<size_t> offset,
                                      std::optional<size_t> limit,
                                      const std::string& workspace_root) {
    std::string abs_path = normalize_path(path);
    std::error_code ec;
    std::string canon_root = fs::canonical(workspace_root, ec).string();
    if (ec) canon_root = workspace_root;

    if (!validate_workspace_boundary(abs_path, canon_root)) {
        throw std::runtime_error(
            "path " + abs_path + " escapes workspace boundary " + canon_root);
    }
    return read_file(path, offset, limit);
}

WriteFileOutput write_file_in_workspace(const std::string& path,
                                        const std::string& content,
                                        const std::string& workspace_root) {
    std::string abs_path = normalize_path_allow_missing(path);
    std::error_code ec;
    std::string canon_root = fs::canonical(workspace_root, ec).string();
    if (ec) canon_root = workspace_root;

    if (!validate_workspace_boundary(abs_path, canon_root)) {
        throw std::runtime_error(
            "path " + abs_path + " escapes workspace boundary " + canon_root);
    }
    return write_file(path, content);
}

}  // namespace fliq
