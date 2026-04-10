#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::command_exec — Safe command execution with output capture
// Translated from Rust runtime/bash.rs
// ─────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fliq {

// Maximum output bytes before truncation (16 KiB, matching upstream).
constexpr size_t MAX_OUTPUT_BYTES = 16384;

struct CommandInput {
    std::string              command;
    std::optional<uint64_t>  timeout_ms;
    std::optional<std::string> description;
    bool                     run_in_background = false;
    std::optional<std::string> working_directory;
};

struct CommandOutput {
    std::string              stdout_str;
    std::string              stderr_str;
    bool                     interrupted    = false;
    int                      exit_code      = 0;
    std::optional<std::string> background_task_id;
    std::optional<std::string> return_code_interpretation;
    bool                     no_output_expected = false;
};

// ── Core API ─────────────────────────────────────────────────────────

/// Execute a shell command and capture stdout/stderr.
/// On Windows uses cmd.exe /c, on Unix uses sh -lc.
CommandOutput execute_command(const CommandInput& input);

/// Truncate output string to MAX_OUTPUT_BYTES, appending a marker.
std::string truncate_output(const std::string& s);

}  // namespace fliq
