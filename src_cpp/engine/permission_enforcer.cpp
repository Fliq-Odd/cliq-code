// ─────────────────────────────────────────────────────────────────────
// fliq::permission_enforcer — Implementation
// Translated from Rust crates/runtime/src/permission_enforcer.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/permission_enforcer.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace fliq {

// ── Mode string conversion ──────────────────────────────────────────

const char* permission_mode_str(PermissionMode mode) {
    switch (mode) {
        case PermissionMode::ReadOnly:         return "read-only";
        case PermissionMode::WorkspaceWrite:   return "workspace-write";
        case PermissionMode::Prompt:           return "prompt";
        case PermissionMode::Allow:            return "allow";
        case PermissionMode::DangerFullAccess: return "danger-full-access";
    }
    return "unknown";
}

// ── Read-only command heuristic ─────────────────────────────────────

bool is_read_only_command(const std::string& command) {
    // Extract the first token, stripping path prefix
    auto first_space = command.find(' ');
    std::string first_token = (first_space != std::string::npos)
        ? command.substr(0, first_space)
        : command;

    // Strip path prefix (like /usr/bin/cat → cat)
    auto last_slash = first_token.rfind('/');
    if (last_slash != std::string::npos) {
        first_token = first_token.substr(last_slash + 1);
    }
    auto last_backslash = first_token.rfind('\\');
    if (last_backslash != std::string::npos) {
        first_token = first_token.substr(last_backslash + 1);
    }

    if (first_token.empty()) return false;

    // Allowlisted read-only commands (matching Rust source exactly)
    static const std::unordered_set<std::string> safe_commands = {
        "cat", "head", "tail", "less", "more", "wc", "ls", "find",
        "grep", "rg", "awk", "sed", "echo", "printf", "which", "where",
        "whoami", "pwd", "env", "printenv", "date", "cal", "df", "du",
        "free", "uptime", "uname", "file", "stat", "diff", "sort",
        "uniq", "tr", "cut", "paste", "tee", "xargs", "test", "true",
        "false", "type", "readlink", "realpath", "basename", "dirname",
        "sha256sum", "md5sum", "b3sum", "xxd", "hexdump", "od",
        "strings", "tree", "jq", "yq", "python3", "python", "node",
        "ruby", "cargo", "rustc", "git", "gh",
        // Windows equivalents
        "dir", "findstr", "where.exe", "type", "powershell",
    };

    if (safe_commands.find(first_token) == safe_commands.end()) {
        return false;
    }

    // Block redirects and in-place modifications
    if (command.find("-i ") != std::string::npos) return false;
    if (command.find("--in-place") != std::string::npos) return false;
    if (command.find(" > ") != std::string::npos) return false;
    if (command.find(" >> ") != std::string::npos) return false;

    return true;
}

// ── Workspace boundary check ────────────────────────────────────────

bool is_within_workspace(const std::string& path,
                         const std::string& workspace_root) {
    std::string normalized = path;
    if (!path.empty() && path[0] != '/' && path[0] != '\\' &&
        (path.size() < 2 || path[1] != ':')) {
        // Relative path: prepend workspace root
        normalized = workspace_root + "/" + path;
    }

    std::string root = workspace_root;
    if (!root.empty() && root.back() != '/' && root.back() != '\\') {
        root += '/';
    }

    // Normalize slashes for comparison
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::replace(root.begin(), root.end(), '\\', '/');

    // Trim trailing slash for exact-match comparison
    std::string root_trimmed = workspace_root;
    while (!root_trimmed.empty() &&
           (root_trimmed.back() == '/' || root_trimmed.back() == '\\')) {
        root_trimmed.pop_back();
    }
    std::replace(root_trimmed.begin(), root_trimmed.end(), '\\', '/');

    return normalized.rfind(root, 0) == 0 || normalized == root_trimmed;
}

// ── PermissionEnforcer ──────────────────────────────────────────────

PermissionEnforcer::PermissionEnforcer(PermissionMode mode)
    : mode_(mode) {}

EnforcementResult PermissionEnforcer::check(const std::string& tool_name,
                                            const std::string& /*input*/) const {
    // Prompt mode defers to interactive flow
    if (mode_ == PermissionMode::Prompt) {
        return std::monostate{};  // Allowed
    }
    if (mode_ == PermissionMode::Allow ||
        mode_ == PermissionMode::DangerFullAccess) {
        return std::monostate{};  // Allowed
    }
    // ReadOnly mode: deny write tools
    if (mode_ == PermissionMode::ReadOnly) {
        static const std::unordered_set<std::string> write_tools = {
            "write_file", "edit_file", "bash",
        };
        if (write_tools.count(tool_name)) {
            return DeniedInfo{
                tool_name,
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::WorkspaceWrite),
                tool_name + " requires workspace-write permission",
            };
        }
    }
    return std::monostate{};  // Allowed
}

EnforcementResult PermissionEnforcer::check_file_write(
    const std::string& path,
    const std::string& workspace_root) const {

    switch (mode_) {
        case PermissionMode::ReadOnly:
            return DeniedInfo{
                "write_file",
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::WorkspaceWrite),
                std::string("file writes are not allowed in '") +
                    permission_mode_str(mode_) + "' mode",
            };

        case PermissionMode::WorkspaceWrite:
            if (is_within_workspace(path, workspace_root)) {
                return std::monostate{};
            }
            return DeniedInfo{
                "write_file",
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::DangerFullAccess),
                "path '" + path + "' is outside workspace root '" +
                    workspace_root + "'",
            };

        case PermissionMode::Allow:
        case PermissionMode::DangerFullAccess:
            return std::monostate{};

        case PermissionMode::Prompt:
            return DeniedInfo{
                "write_file",
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::WorkspaceWrite),
                "file write requires confirmation in prompt mode",
            };
    }
    return std::monostate{};
}

EnforcementResult PermissionEnforcer::check_bash(
    const std::string& command) const {

    switch (mode_) {
        case PermissionMode::ReadOnly:
            if (is_read_only_command(command)) {
                return std::monostate{};
            }
            return DeniedInfo{
                "bash",
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::WorkspaceWrite),
                std::string("command may modify state; not allowed in '") +
                    permission_mode_str(mode_) + "' mode",
            };

        case PermissionMode::Prompt:
            return DeniedInfo{
                "bash",
                permission_mode_str(mode_),
                permission_mode_str(PermissionMode::DangerFullAccess),
                "bash requires confirmation in prompt mode",
            };

        default:
            return std::monostate{};  // WorkspaceWrite, Allow, DangerFullAccess
    }
}

}  // namespace fliq
