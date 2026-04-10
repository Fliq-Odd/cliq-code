#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::bash_validation — Full command validation pipeline
// Translated from Rust runtime/bash_validation.rs
// ─────────────────────────────────────────────────────────────────────
#include <string>

#include "fliq/permission_enforcer.hpp"

namespace fliq {

enum class ValidationResult { Allow, Block, Warn };
struct ValidationOutcome {
    ValidationResult result = ValidationResult::Allow;
    std::string      message;
};

enum class CommandIntent {
    ReadOnly, Write, Destructive, Network,
    ProcessManagement, PackageManagement, SystemAdmin, Unknown,
};

// ── Validation pipeline ──────────────────────────────────────────────
ValidationOutcome validate_read_only(const std::string& command, PermissionMode mode);
ValidationOutcome check_destructive(const std::string& command);
ValidationOutcome validate_mode(const std::string& command, PermissionMode mode);
ValidationOutcome validate_sed(const std::string& command, PermissionMode mode);
ValidationOutcome validate_paths(const std::string& command, const std::string& workspace);
ValidationOutcome validate_command(const std::string& command, PermissionMode mode, const std::string& workspace);
CommandIntent     classify_command(const std::string& command);
std::string       extract_first_command(const std::string& command);

}  // namespace fliq
