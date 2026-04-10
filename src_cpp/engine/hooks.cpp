// ─────────────────────────────────────────────────────────────────────
// fliq::hooks — Pre/post tool-use hook execution pipeline
// Translated from Rust crates/runtime/src/hooks.rs (988 LOC)
// ─────────────────────────────────────────────────────────────────────

#include "fliq/hooks.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fliq {

const char* hook_event_str(HookEvent e) {
    switch (e) {
        case HookEvent::PreToolUse:        return "PreToolUse";
        case HookEvent::PostToolUse:       return "PostToolUse";
        case HookEvent::PostToolUseFailure: return "PostToolUseFailure";
    }
    return "Unknown";
}

// ── Build JSON payload for hook stdin ────────────────────────────────

static std::string build_payload(HookEvent event, const std::string& tool,
                                  const std::string& input, const std::string* output,
                                  bool is_error) {
    std::ostringstream ss;
    ss << "{\"hook_event_name\":\"" << hook_event_str(event) << "\""
       << ",\"tool_name\":\"" << tool << "\""
       << ",\"tool_input_json\":\"" << input << "\""
       << ",\"tool_result_is_error\":" << (is_error ? "true" : "false");
    if (output) {
        if (event == HookEvent::PostToolUseFailure)
            ss << ",\"tool_error\":\"" << *output << "\"";
        else
            ss << ",\"tool_output\":\"" << *output << "\"";
    }
    ss << "}";
    return ss.str();
}

// ── Parse hook stdout for decisions ──────────────────────────────────

struct ParsedOutput {
    std::vector<std::string> messages;
    bool deny = false;
    std::optional<std::string> updated_input;
};

static ParsedOutput parse_hook_output(const std::string& stdout_str) {
    ParsedOutput parsed;
    if (stdout_str.empty()) return parsed;

    // Try to find systemMessage, reason, continue, decision fields
    // (simplified JSON field extraction — avoids needing full JSON parser dependency)
    auto extract_field = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = stdout_str.find(search);
        if (pos == std::string::npos) return "";
        auto start = pos + search.size();
        auto end = stdout_str.find("\"", start);
        if (end == std::string::npos) return "";
        return stdout_str.substr(start, end - start);
    };

    auto sys_msg = extract_field("systemMessage");
    if (!sys_msg.empty()) parsed.messages.push_back(sys_msg);

    auto reason = extract_field("reason");
    if (!reason.empty()) parsed.messages.push_back(reason);

    // Check for deny signals
    if (stdout_str.find("\"continue\":false") != std::string::npos ||
        stdout_str.find("\"decision\":\"block\"") != std::string::npos) {
        parsed.deny = true;
    }

    // Check for updated input
    auto updated = extract_field("updatedInput");
    if (!updated.empty()) parsed.updated_input = updated;

    if (parsed.messages.empty() && !stdout_str.empty())
        parsed.messages.push_back(stdout_str);

    return parsed;
}

// ── Execute a single hook command ────────────────────────────────────

struct CommandResult {
    int exit_code = -1;
    std::string stdout_str;
    std::string stderr_str;
    bool cancelled = false;
    bool spawn_failed = false;
};

static CommandResult run_single_command(const std::string& command,
                                         const std::string& payload,
                                         HookEvent event,
                                         const std::string& tool_name,
                                         const std::string& tool_input,
                                         const std::string* tool_output,
                                         bool is_error,
                                         HookAbortSignal* signal) {
    CommandResult result;

#ifdef _WIN32
    // Windows: use cmd /C with environment variables
    // Create temp file for payload
    char tmp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp_path);
    std::string tmp_file = std::string(tmp_path) + "fliq_hook_payload.tmp";
    {
        FILE* f = fopen(tmp_file.c_str(), "w");
        if (f) { fprintf(f, "%s", payload.c_str()); fclose(f); }
    }

    std::string full_cmd = "cmd /C \"set HOOK_EVENT=" + std::string(hook_event_str(event))
        + " && set HOOK_TOOL_NAME=" + tool_name
        + " && set HOOK_TOOL_IS_ERROR=" + (is_error ? "1" : "0")
        + " && " + command + " < " + tmp_file + "\"";

    FILE* pipe = _popen(full_cmd.c_str(), "r");
    if (!pipe) { result.spawn_failed = true; return result; }

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        if (signal && signal->is_aborted()) {
            _pclose(pipe);
            result.cancelled = true;
            return result;
        }
        result.stdout_str += buf;
    }
    result.exit_code = _pclose(pipe);
    DeleteFileA(tmp_file.c_str());
#else
    // POSIX: use sh -lc with environment and pipe stdin
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) || pipe(stdout_pipe) || pipe(stderr_pipe)) {
        result.spawn_failed = true;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) { result.spawn_failed = true; return result; }

    if (pid == 0) {
        // Child
        close(stdin_pipe[1]); close(stdout_pipe[0]); close(stderr_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]); close(stdout_pipe[1]); close(stderr_pipe[1]);

        setenv("HOOK_EVENT", hook_event_str(event), 1);
        setenv("HOOK_TOOL_NAME", tool_name.c_str(), 1);
        setenv("HOOK_TOOL_INPUT", tool_input.c_str(), 1);
        setenv("HOOK_TOOL_IS_ERROR", is_error ? "1" : "0", 1);
        if (tool_output) setenv("HOOK_TOOL_OUTPUT", tool_output->c_str(), 1);

        execl("/bin/sh", "sh", "-lc", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(stdin_pipe[0]); close(stdout_pipe[1]); close(stderr_pipe[1]);

    // Write payload to stdin
    write(stdin_pipe[1], payload.c_str(), payload.size());
    close(stdin_pipe[1]);

    // Read stdout/stderr with abort polling
    char buf[4096];
    while (true) {
        if (signal && signal->is_aborted()) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            close(stdout_pipe[0]); close(stderr_pipe[0]);
            result.cancelled = true;
            return result;
        }
        ssize_t n = read(stdout_pipe[0], buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        result.stdout_str += buf;
    }
    close(stdout_pipe[0]);

    ssize_t n = read(stderr_pipe[0], buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; result.stderr_str = buf; }
    close(stderr_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    // Trim whitespace
    while (!result.stdout_str.empty() &&
           (result.stdout_str.back() == '\n' || result.stdout_str.back() == '\r' ||
            result.stdout_str.back() == ' '))
        result.stdout_str.pop_back();

    return result;
}

// ── HookRunner ───────────────────────────────────────────────────────

HookRunner::HookRunner(const HookConfig& config) : config_(config) {}

HookRunResult HookRunner::run_pre_tool_use(const std::string& tool, const std::string& input,
                                             HookAbortSignal* signal) const {
    return run_commands(HookEvent::PreToolUse, config_.pre_tool_use, tool, input, nullptr, false, signal);
}

HookRunResult HookRunner::run_post_tool_use(const std::string& tool, const std::string& input,
                                              const std::string& output, bool is_error,
                                              HookAbortSignal* signal) const {
    return run_commands(HookEvent::PostToolUse, config_.post_tool_use, tool, input, &output, is_error, signal);
}

HookRunResult HookRunner::run_post_tool_use_failure(const std::string& tool, const std::string& input,
                                                      const std::string& error_msg,
                                                      HookAbortSignal* signal) const {
    return run_commands(HookEvent::PostToolUseFailure, config_.post_tool_use_failure, tool, input, &error_msg, true, signal);
}

HookRunResult HookRunner::run_commands(HookEvent event, const std::vector<std::string>& commands,
                                         const std::string& tool, const std::string& input,
                                         const std::string* output, bool is_error,
                                         HookAbortSignal* signal) const {
    HookRunResult result;
    if (commands.empty()) return result;

    if (signal && signal->is_aborted()) {
        result.cancelled = true;
        result.messages.push_back(std::string(hook_event_str(event)) + " hook cancelled before execution");
        return result;
    }

    auto payload = build_payload(event, tool, input, output, is_error);

    for (auto& cmd : commands) {
        auto cmd_result = run_single_command(cmd, payload, event, tool, input, output, is_error, signal);

        if (cmd_result.cancelled) {
            result.cancelled = true;
            result.messages.push_back(std::string(hook_event_str(event)) + " hook `" + cmd + "` cancelled");
            return result;
        }

        if (cmd_result.spawn_failed) {
            result.failed = true;
            result.messages.push_back(std::string(hook_event_str(event)) + " hook `" + cmd + "` failed to start");
            return result;
        }

        auto parsed = parse_hook_output(cmd_result.stdout_str);
        result.messages.insert(result.messages.end(), parsed.messages.begin(), parsed.messages.end());
        if (parsed.updated_input) result.updated_input = parsed.updated_input;

        if (cmd_result.exit_code == 0) {
            if (parsed.deny) { result.denied = true; return result; }
        } else if (cmd_result.exit_code == 2) {
            result.denied = true;
            if (result.messages.empty())
                result.messages.push_back(std::string(hook_event_str(event)) + " hook denied tool `" + tool + "`");
            return result;
        } else {
            result.failed = true;
            if (result.messages.empty()) {
                std::ostringstream msg;
                msg << "Hook `" << cmd << "` exited with status " << cmd_result.exit_code;
                if (!cmd_result.stderr_str.empty()) msg << ": " << cmd_result.stderr_str;
                result.messages.push_back(msg.str());
            }
            return result;
        }
    }

    return result;
}

}  // namespace fliq
