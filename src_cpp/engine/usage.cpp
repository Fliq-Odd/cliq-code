// ─────────────────────────────────────────────────────────────────────
// fliq::usage — Implementation
// Translated from Rust crates/runtime/src/usage.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/usage.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>

namespace fliq {

ModelPricing ModelPricing::default_sonnet_tier() {
    return {15.0, 75.0, 18.75, 1.5};
}

uint32_t TokenUsage::total_tokens() const {
    return input_tokens + output_tokens + cache_creation_input_tokens + cache_read_input_tokens;
}

double UsageCostEstimate::total_cost_usd() const {
    return input_cost_usd + output_cost_usd + cache_creation_cost_usd + cache_read_cost_usd;
}

static double cost_for_tokens(uint32_t tokens, double usd_per_million) {
    return static_cast<double>(tokens) / 1'000'000.0 * usd_per_million;
}

UsageCostEstimate estimate_cost(const TokenUsage& u, const ModelPricing& p) {
    return {
        cost_for_tokens(u.input_tokens, p.input_cost_per_million),
        cost_for_tokens(u.output_tokens, p.output_cost_per_million),
        cost_for_tokens(u.cache_creation_input_tokens, p.cache_creation_cost_per_million),
        cost_for_tokens(u.cache_read_input_tokens, p.cache_read_cost_per_million),
    };
}

std::optional<ModelPricing> pricing_for_model(const std::string& model) {
    std::string lower = model;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("haiku") != std::string::npos)
        return ModelPricing{1.0, 5.0, 1.25, 0.1};
    if (lower.find("opus") != std::string::npos)
        return ModelPricing{15.0, 75.0, 18.75, 1.5};
    if (lower.find("sonnet") != std::string::npos)
        return ModelPricing::default_sonnet_tier();
    return std::nullopt;
}

std::string format_usd(double amount) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "$%.4f", amount);
    return buf;
}

std::vector<std::string> usage_summary_lines(const TokenUsage& u, const std::string& label, const std::string& model) {
    auto pricing = model.empty() ? std::nullopt : pricing_for_model(model);
    auto cost = pricing ? estimate_cost(u, *pricing) : estimate_cost(u);

    std::string model_suffix = model.empty() ? "" : " model=" + model;
    std::string pricing_suffix = "";
    if (!model.empty() && !pricing) pricing_suffix = " pricing=estimated-default";

    std::ostringstream line1;
    line1 << label << ": total_tokens=" << u.total_tokens()
          << " input=" << u.input_tokens
          << " output=" << u.output_tokens
          << " cache_write=" << u.cache_creation_input_tokens
          << " cache_read=" << u.cache_read_input_tokens
          << " estimated_cost=" << format_usd(cost.total_cost_usd())
          << model_suffix << pricing_suffix;

    std::ostringstream line2;
    line2 << "  cost breakdown: input=" << format_usd(cost.input_cost_usd)
          << " output=" << format_usd(cost.output_cost_usd)
          << " cache_write=" << format_usd(cost.cache_creation_cost_usd)
          << " cache_read=" << format_usd(cost.cache_read_cost_usd);

    return {line1.str(), line2.str()};
}

void UsageTracker::record(const TokenUsage& usage) {
    latest_turn_ = usage;
    cumulative_.input_tokens += usage.input_tokens;
    cumulative_.output_tokens += usage.output_tokens;
    cumulative_.cache_creation_input_tokens += usage.cache_creation_input_tokens;
    cumulative_.cache_read_input_tokens += usage.cache_read_input_tokens;
    ++turns_;
}

}  // namespace fliq
