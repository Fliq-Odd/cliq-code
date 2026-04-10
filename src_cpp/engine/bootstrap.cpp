// ─────────────────────────────────────────────────────────────────────
// fliq::bootstrap — Implementation
// ─────────────────────────────────────────────────────────────────────

#include "fliq/bootstrap.hpp"
#include <algorithm>

namespace fliq {

BootstrapPlan BootstrapPlan::default_plan() {
    return from_phases({
        BootstrapPhase::CliEntry,
        BootstrapPhase::FastPathVersion,
        BootstrapPhase::StartupProfiler,
        BootstrapPhase::SystemPromptFastPath,
        BootstrapPhase::ChromeMcpFastPath,
        BootstrapPhase::DaemonWorkerFastPath,
        BootstrapPhase::BridgeFastPath,
        BootstrapPhase::DaemonFastPath,
        BootstrapPhase::BackgroundSessionFastPath,
        BootstrapPhase::TemplateFastPath,
        BootstrapPhase::EnvironmentRunnerFastPath,
        BootstrapPhase::MainRuntime,
    });
}

BootstrapPlan BootstrapPlan::from_phases(std::vector<BootstrapPhase> phases) {
    std::vector<BootstrapPhase> deduped;
    for (auto p : phases) {
        if (std::find(deduped.begin(), deduped.end(), p) == deduped.end())
            deduped.push_back(p);
    }
    return BootstrapPlan(std::move(deduped));
}

}  // namespace fliq
