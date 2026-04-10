// ─────────────────────────────────────────────────────────────────────
// fliq::config — Multi-source config loader & merger
// Translated from Rust crates/runtime/src/config.rs (1533 LOC)
// ─────────────────────────────────────────────────────────────────────

#include "fliq/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fliq {
namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────

static std::string read_file_if_exists(const std::string& path) {
    if (!fs::exists(path)) return "";
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

// Simple JSON key-value extraction (supports flat configs)
static std::map<std::string, std::string> parse_flat_json(const std::string& content) {
    std::map<std::string, std::string> result;
    if (content.empty()) return result;

    // Find all "key":"value" pairs
    size_t pos = 0;
    while (pos < content.size()) {
        auto key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        auto key_end = content.find('"', key_start + 1);
        if (key_end == std::string::npos) break;

        auto colon = content.find(':', key_end + 1);
        if (colon == std::string::npos) break;

        // Skip whitespace after colon
        auto val_start = colon + 1;
        while (val_start < content.size() && content[val_start] == ' ') ++val_start;

        std::string key = content.substr(key_start + 1, key_end - key_start - 1);
        std::string value;

        if (val_start < content.size() && content[val_start] == '"') {
            // String value
            auto val_end = content.find('"', val_start + 1);
            if (val_end != std::string::npos) {
                value = content.substr(val_start + 1, val_end - val_start - 1);
                pos = val_end + 1;
            } else {
                pos = val_start + 1;
                continue;
            }
        } else {
            // Non-string value (bool, number)
            auto val_end = content.find_first_of(",}\n", val_start);
            if (val_end == std::string::npos) val_end = content.size();
            value = content.substr(val_start, val_end - val_start);
            // Trim
            while (!value.empty() && (value.back() == ' ' || value.back() == '\r')) value.pop_back();
            pos = val_end;
        }

        result[key] = value;
    }
    return result;
}

// Extract string array from JSON: "key": ["a", "b", "c"]
static std::vector<std::string> extract_string_array(const std::string& content, const std::string& key) {
    std::vector<std::string> result;
    std::string search = "\"" + key + "\"";
    auto pos = content.find(search);
    if (pos == std::string::npos) return result;

    auto bracket = content.find('[', pos);
    if (bracket == std::string::npos) return result;
    auto end_bracket = content.find(']', bracket);
    if (end_bracket == std::string::npos) return result;

    auto arr_content = content.substr(bracket + 1, end_bracket - bracket - 1);
    size_t i = 0;
    while (i < arr_content.size()) {
        auto q1 = arr_content.find('"', i);
        if (q1 == std::string::npos) break;
        auto q2 = arr_content.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(arr_content.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return result;
}

// ── Default config home ──────────────────────────────────────────────

std::string RuntimeConfig::default_config_home() {
    // Check FLIQ_CONFIG_HOME env
    if (auto* env = std::getenv("FLIQ_CONFIG_HOME")) return env;
    // Check CLAW_CONFIG_HOME for compat
    if (auto* env = std::getenv("CLAW_CONFIG_HOME")) return env;

#ifdef _WIN32
    if (auto* appdata = std::getenv("APPDATA"))
        return std::string(appdata) + "\\fliq";
    if (auto* home = std::getenv("USERPROFILE"))
        return std::string(home) + "\\.fliq";
#else
    if (auto* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::string(xdg) + "/fliq";
    if (auto* home = std::getenv("HOME"))
        return std::string(home) + "/.fliq";
#endif
    return ".fliq";
}

// ── Config file discovery ────────────────────────────────────────────

std::vector<ConfigEntry> RuntimeConfig::discover(const std::string& cwd, const std::string& config_home) {
    return {
        // User-level config
        {ConfigSource::User, config_home + "/settings.json"},
        // Project-level config (workspace root)
        {ConfigSource::Project, cwd + "/.fliq.json"},
        {ConfigSource::Project, cwd + "/.fliq/settings.json"},
        // Local overrides (gitignored)
        {ConfigSource::Local,  cwd + "/.fliq/settings.local.json"},
    };
}

// ── Load and merge ───────────────────────────────────────────────────

RuntimeConfig RuntimeConfig::empty() {
    return RuntimeConfig{};
}

RuntimeConfig RuntimeConfig::load(const std::string& cwd) {
    return load_from(cwd, default_config_home());
}

RuntimeConfig RuntimeConfig::load_from(const std::string& cwd, const std::string& config_home) {
    RuntimeConfig config;
    auto entries = discover(cwd, config_home);

    for (auto& entry : entries) {
        auto content = read_file_if_exists(entry.path);
        if (content.empty()) continue;

        auto parsed = parse_flat_json(content);
        for (auto& [k, v] : parsed) config.merged_[k] = v;
        config.loaded_entries_.push_back(entry);

        // Extract hooks config
        auto pre = extract_string_array(content, "PreToolUse");
        auto post = extract_string_array(content, "PostToolUse");
        auto post_fail = extract_string_array(content, "PostToolUseFailure");
        if (!pre.empty()) config.feature_config_.hooks.pre_tool_use = std::move(pre);
        if (!post.empty()) config.feature_config_.hooks.post_tool_use = std::move(post);
        if (!post_fail.empty()) config.feature_config_.hooks.post_tool_use_failure = std::move(post_fail);

        // Extract permission rules
        auto allow = extract_string_array(content, "allow");
        auto deny = extract_string_array(content, "deny");
        auto ask = extract_string_array(content, "ask");
        if (!allow.empty()) config.feature_config_.permission_rules.allow = std::move(allow);
        if (!deny.empty()) config.feature_config_.permission_rules.deny = std::move(deny);
        if (!ask.empty()) config.feature_config_.permission_rules.ask = std::move(ask);
    }

    // Extract model
    auto it = config.merged_.find("model");
    if (it != config.merged_.end()) config.feature_config_.model = it->second;

    return config;
}

// ── Accessors ────────────────────────────────────────────────────────

std::optional<std::string> RuntimeConfig::get(const std::string& key) const {
    auto it = merged_.find(key);
    return it != merged_.end() ? std::optional<std::string>(it->second) : std::nullopt;
}

void RuntimeConfig::set(const std::string& key, const std::string& value) {
    merged_[key] = value;
}

}  // namespace fliq
