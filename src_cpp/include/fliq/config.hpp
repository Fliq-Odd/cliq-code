#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::config — Multi-source runtime configuration system
// Translated from Rust runtime/config.rs (1533 LOC)
// ─────────────────────────────────────────────────────────────────────
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fliq/hooks.hpp"

namespace fliq {

enum class ConfigSource { User, Project, Local };

struct ConfigEntry {
    ConfigSource source;
    std::string  path;
};

struct PermissionRuleConfig {
    std::vector<std::string> allow;
    std::vector<std::string> deny;
    std::vector<std::string> ask;
};

struct RuntimeFeatureConfig {
    HookConfig                 hooks;
    std::optional<std::string> model;
    PermissionRuleConfig       permission_rules;
};

class RuntimeConfig {
public:
    RuntimeConfig() = default;
    static RuntimeConfig empty();

    // Accessor methods
    const std::map<std::string, std::string>& merged() const { return merged_; }
    const std::vector<ConfigEntry>& loaded_entries() const { return loaded_entries_; }
    const RuntimeFeatureConfig& feature_config() const { return feature_config_; }
    const HookConfig& hooks() const { return feature_config_.hooks; }
    std::optional<std::string> model() const { return feature_config_.model; }
    const PermissionRuleConfig& permission_rules() const { return feature_config_.permission_rules; }

    std::optional<std::string> get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);

    // Config file locations
    static std::string default_config_home();
    static std::vector<ConfigEntry> discover(const std::string& cwd, const std::string& config_home);

    // Load and merge
    static RuntimeConfig load(const std::string& cwd);
    static RuntimeConfig load_from(const std::string& cwd, const std::string& config_home);

private:
    std::map<std::string, std::string> merged_;
    std::vector<ConfigEntry>           loaded_entries_;
    RuntimeFeatureConfig               feature_config_;
};

}  // namespace fliq
