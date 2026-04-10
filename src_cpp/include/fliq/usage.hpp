#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::usage — Token usage tracking & cost estimation
// Translated from Rust runtime/usage.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fliq {

struct ModelPricing {
    double input_cost_per_million         = 15.0;
    double output_cost_per_million        = 75.0;
    double cache_creation_cost_per_million = 18.75;
    double cache_read_cost_per_million    = 1.5;
    static ModelPricing default_sonnet_tier();
};

struct TokenUsage {
    uint32_t input_tokens                  = 0;
    uint32_t output_tokens                 = 0;
    uint32_t cache_creation_input_tokens   = 0;
    uint32_t cache_read_input_tokens       = 0;

    uint32_t total_tokens() const;
};

struct UsageCostEstimate {
    double input_cost_usd          = 0.0;
    double output_cost_usd         = 0.0;
    double cache_creation_cost_usd = 0.0;
    double cache_read_cost_usd     = 0.0;
    double total_cost_usd() const;
};

UsageCostEstimate  estimate_cost(const TokenUsage& usage, const ModelPricing& pricing = ModelPricing::default_sonnet_tier());
std::optional<ModelPricing> pricing_for_model(const std::string& model);
std::string        format_usd(double amount);
std::vector<std::string> usage_summary_lines(const TokenUsage& usage, const std::string& label, const std::string& model = "");

class UsageTracker {
public:
    UsageTracker() = default;
    void       record(const TokenUsage& usage);
    TokenUsage current_turn_usage() const { return latest_turn_; }
    TokenUsage cumulative_usage()   const { return cumulative_; }
    uint32_t   turns()              const { return turns_; }
private:
    TokenUsage latest_turn_{};
    TokenUsage cumulative_{};
    uint32_t   turns_ = 0;
};

}  // namespace fliq
