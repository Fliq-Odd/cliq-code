#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::permission_enforcer — Permission enforcement layer
// Translated from Rust runtime/permission_enforcer.rs
// ─────────────────────────────────────────────────────────────────────

#include <string>
#include <unordered_set>
#include <variant>

namespace fliq {

// ── Permission modes ─────────────────────────────────────────────────
enum class PermissionMode {
    ReadOnly,
    WorkspaceWrite,
    Prompt,
    Allow,
    DangerFullAccess,
};

const char* permission_mode_str(PermissionMode mode);

// ── Enforcement result ───────────────────────────────────────────────
struct DeniedInfo {
    std::string tool;
    std::string active_mode;
    std::string required_mode;
    std::string reason;
};

// Allowed = std::monostate, Denied = DeniedInfo
using EnforcementResult = std::variant<std::monostate, DeniedInfo>;

inline bool is_allowed(const EnforcementResult& r) {
    return std::holds_alternative<std::monostate>(r);
}

// ── Enforcer ─────────────────────────────────────────────────────────
class PermissionEnforcer {
public:
    explicit PermissionEnforcer(PermissionMode mode);

    /// Check whether a tool can be executed.
    EnforcementResult check(const std::string& tool_name,
                            const std::string& input) const;

    /// Check whether a file write is allowed within the workspace.
    EnforcementResult check_file_write(const std::string& path,
                                       const std::string& workspace_root) const;

    /// Check whether a bash command is allowed.
    EnforcementResult check_bash(const std::string& command) const;

    PermissionMode active_mode() const { return mode_; }

private:
    PermissionMode mode_;
};

// ── Heuristics ───────────────────────────────────────────────────────

/// Is the bash command read-only? (conservative heuristic)
bool is_read_only_command(const std::string& command);

/// Is the given path within the workspace root?
bool is_within_workspace(const std::string& path,
                         const std::string& workspace_root);

}  // namespace fliq
