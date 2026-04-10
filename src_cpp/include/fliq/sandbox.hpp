#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::sandbox — Sandbox configuration & status resolution
// Translated from Rust runtime/sandbox.rs
// ─────────────────────────────────────────────────────────────────────
#include <optional>
#include <string>

namespace fliq {

enum class SandboxType { None, Container, Macos, Linux };
enum class SandboxStatus { NotSandboxed, Sandboxed, Error };

struct SandboxConfig {
    SandboxType type    = SandboxType::None;
    bool        enabled = false;
    std::optional<std::string> image_name;
};

struct SandboxResolution {
    SandboxStatus status = SandboxStatus::NotSandboxed;
    SandboxType   type   = SandboxType::None;
    std::string   message;
};

SandboxResolution resolve_sandbox(const SandboxConfig& config);
bool              is_inside_container();
const char*       sandbox_type_str(SandboxType t);
const char*       sandbox_status_str(SandboxStatus s);

}  // namespace fliq
