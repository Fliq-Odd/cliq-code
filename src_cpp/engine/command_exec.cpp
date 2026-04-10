// ─────────────────────────────────────────────────────────────────────
// fliq::command_exec — Implementation
// Translated from Rust crates/runtime/src/bash.rs
// Cross-platform: uses cmd.exe on Windows, sh on Unix
// ─────────────────────────────────────────────────────────────────────

#include "fliq/command_exec.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

namespace fliq {

// ── Output truncation (mirrors Rust truncate_output) ─────────────────

std::string truncate_output(const std::string& s) {
    if (s.size() <= MAX_OUTPUT_BYTES) {
        return s;
    }

    // Find the last valid UTF-8 boundary at or before MAX_OUTPUT_BYTES
    size_t end = MAX_OUTPUT_BYTES;
    while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0u) == 0x80u) {
        --end;
    }

    std::string truncated = s.substr(0, end);
    truncated += "\n\n[output truncated — exceeded 16384 bytes]";
    return truncated;
}

// ── Cross-platform command execution ─────────────────────────────────

#ifdef _WIN32

static CommandOutput execute_windows(const CommandInput& input) {
    std::string cwd = input.working_directory.value_or(
        std::filesystem::current_path().string());

    // Build the full command line for cmd.exe
    std::string cmd_line = "cmd.exe /c " + input.command;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read  = nullptr, stdout_write = nullptr;
    HANDLE stderr_read  = nullptr, stderr_write = nullptr;

    CreatePipe(&stdout_read, &stdout_write, &sa, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    CreatePipe(&stderr_read, &stderr_write, &sa, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError  = stderr_write;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(cmd_line.c_str()),
        nullptr, nullptr, TRUE, 0, nullptr,
        cwd.c_str(), &si, &pi);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        throw std::runtime_error("failed to create process: " + input.command);
    }

    // Handle background tasks
    if (input.run_in_background) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        CommandOutput out;
        out.background_task_id = std::to_string(pi.dwProcessId);
        out.no_output_expected = true;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return out;
    }

    // Wait with optional timeout
    DWORD wait_ms = input.timeout_ms.has_value()
        ? static_cast<DWORD>(input.timeout_ms.value())
        : INFINITE;

    DWORD wait_result = WaitForSingleObject(pi.hProcess, wait_ms);

    bool interrupted = (wait_result == WAIT_TIMEOUT);
    if (interrupted) {
        TerminateProcess(pi.hProcess, 1);
    }

    // Read stdout
    std::string stdout_str;
    {
        char buf[4096];
        DWORD bytes_read = 0;
        while (ReadFile(stdout_read, buf, sizeof(buf), &bytes_read, nullptr) &&
               bytes_read > 0) {
            stdout_str.append(buf, bytes_read);
        }
    }
    CloseHandle(stdout_read);

    // Read stderr
    std::string stderr_str;
    {
        char buf[4096];
        DWORD bytes_read = 0;
        while (ReadFile(stderr_read, buf, sizeof(buf), &bytes_read, nullptr) &&
               bytes_read > 0) {
            stderr_str.append(buf, bytes_read);
        }
    }
    CloseHandle(stderr_read);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CommandOutput out;
    out.stdout_str    = truncate_output(stdout_str);
    out.stderr_str    = truncate_output(stderr_str);
    out.interrupted   = interrupted;
    out.exit_code     = static_cast<int>(exit_code);
    out.no_output_expected = out.stdout_str.empty() && out.stderr_str.empty();

    if (interrupted) {
        out.return_code_interpretation = "timeout";
        out.stderr_str = "Command exceeded timeout of " +
                         std::to_string(input.timeout_ms.value()) + " ms";
    } else if (exit_code != 0) {
        out.return_code_interpretation =
            "exit_code:" + std::to_string(exit_code);
    }

    return out;
}

#else  // POSIX

static CommandOutput execute_posix(const CommandInput& input) {
    std::string cwd = input.working_directory.value_or(
        std::filesystem::current_path().string());

    // Build command: sh -lc "command"
    std::string full_cmd = "cd " + cwd + " && " + input.command;

    // Background tasks
    if (input.run_in_background) {
        full_cmd += " &";
        int ret = system(full_cmd.c_str());
        CommandOutput out;
        out.background_task_id = "bg";
        out.no_output_expected = true;
        out.exit_code = ret;
        return out;
    }

    // Use popen for stdout capture
    std::string redirect_cmd = full_cmd + " 2>&1";
    FILE* pipe = popen(redirect_cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("failed to execute command: " + input.command);
    }

    std::string output;
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    CommandOutput out;
    out.stdout_str         = truncate_output(output);
    out.exit_code          = exit_code;
    out.no_output_expected = out.stdout_str.empty();

    if (exit_code != 0) {
        out.return_code_interpretation =
            "exit_code:" + std::to_string(exit_code);
    }

    return out;
}

#endif

// ── Public entry point ───────────────────────────────────────────────

CommandOutput execute_command(const CommandInput& input) {
#ifdef _WIN32
    return execute_windows(input);
#else
    return execute_posix(input);
#endif
}

}  // namespace fliq
