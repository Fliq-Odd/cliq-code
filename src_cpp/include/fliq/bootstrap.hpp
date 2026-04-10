#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::bootstrap — Ordered startup phase sequencing
// Translated from Rust runtime/bootstrap.rs
// ─────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>

namespace fliq {

enum class BootstrapPhase {
    CliEntry, FastPathVersion, StartupProfiler,
    SystemPromptFastPath, ChromeMcpFastPath, DaemonWorkerFastPath,
    BridgeFastPath, DaemonFastPath, BackgroundSessionFastPath,
    TemplateFastPath, EnvironmentRunnerFastPath, MainRuntime,
};

class BootstrapPlan {
public:
    static BootstrapPlan default_plan();
    static BootstrapPlan from_phases(std::vector<BootstrapPhase> phases);
    const std::vector<BootstrapPhase>& phases() const { return phases_; }
private:
    explicit BootstrapPlan(std::vector<BootstrapPhase> p) : phases_(std::move(p)) {}
    std::vector<BootstrapPhase> phases_;
};

}  // namespace fliq
