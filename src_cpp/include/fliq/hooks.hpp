#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::hooks — Pre/post tool-use hook execution pipeline
// Translated from Rust runtime/hooks.rs
// ─────────────────────────────────────────────────────────────────────
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fliq {

enum class HookEvent { PreToolUse, PostToolUse, PostToolUseFailure };
const char* hook_event_str(HookEvent e);

struct HookRunResult {
    bool denied    = false;
    bool failed    = false;
    bool cancelled = false;
    std::vector<std::string>  messages;
    std::optional<std::string> permission_reason;
    std::optional<std::string> updated_input;
};

class HookAbortSignal {
public:
    void abort() { aborted_.store(true, std::memory_order_seq_cst); }
    bool is_aborted() const { return aborted_.load(std::memory_order_seq_cst); }
private:
    std::atomic<bool> aborted_{false};
};

using HookProgressCallback = std::function<void(HookEvent, const std::string& tool, const std::string& cmd)>;

struct HookConfig {
    std::vector<std::string> pre_tool_use;
    std::vector<std::string> post_tool_use;
    std::vector<std::string> post_tool_use_failure;
};

class HookRunner {
public:
    explicit HookRunner(const HookConfig& config);

    HookRunResult run_pre_tool_use(const std::string& tool, const std::string& input,
                                    HookAbortSignal* signal = nullptr) const;
    HookRunResult run_post_tool_use(const std::string& tool, const std::string& input,
                                     const std::string& output, bool is_error,
                                     HookAbortSignal* signal = nullptr) const;
    HookRunResult run_post_tool_use_failure(const std::string& tool, const std::string& input,
                                             const std::string& error_msg,
                                             HookAbortSignal* signal = nullptr) const;
private:
    HookRunResult run_commands(HookEvent event, const std::vector<std::string>& commands,
                                const std::string& tool, const std::string& input,
                                const std::string* output, bool is_error,
                                HookAbortSignal* signal) const;
    HookConfig config_;
};

}  // namespace fliq
