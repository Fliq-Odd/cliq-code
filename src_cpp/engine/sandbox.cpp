// fliq::sandbox — Implementation (Rust sandbox.rs)
#include "fliq/sandbox.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fliq {

const char* sandbox_type_str(SandboxType t) {
    switch (t) {
        case SandboxType::None: return "none";
        case SandboxType::Container: return "container";
        case SandboxType::Macos: return "macos";
        case SandboxType::Linux: return "linux";
    }
    return "unknown";
}

const char* sandbox_status_str(SandboxStatus s) {
    switch (s) {
        case SandboxStatus::NotSandboxed: return "not_sandboxed";
        case SandboxStatus::Sandboxed: return "sandboxed";
        case SandboxStatus::Error: return "error";
    }
    return "unknown";
}

bool is_inside_container() {
    if (std::filesystem::exists("/.dockerenv")) return true;
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line))
            if (line.find("docker") != std::string::npos ||
                line.find("lxc") != std::string::npos ||
                line.find("kubepods") != std::string::npos) return true;
    }
    auto* env = std::getenv("container");
    if (env && std::string(env) == "docker") return true;
    if (std::getenv("KUBERNETES_SERVICE_HOST")) return true;
    return false;
}

SandboxResolution resolve_sandbox(const SandboxConfig& config) {
    if (!config.enabled) {
        if (is_inside_container())
            return {SandboxStatus::Sandboxed, SandboxType::Container, "Auto-detected container"};
        return {SandboxStatus::NotSandboxed, SandboxType::None, "Sandbox disabled"};
    }
    if (config.type == SandboxType::Container)
        return {SandboxStatus::Sandboxed, SandboxType::Container, "Container sandbox"};
    return {SandboxStatus::NotSandboxed, config.type, "Configured sandbox"};
}

}  // namespace fliq
