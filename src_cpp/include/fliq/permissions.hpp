#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::permissions — Rule-based permission policy engine
// Translated from Rust runtime/permissions.rs
// ─────────────────────────────────────────────────────────────────────
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fliq {

// Re-uses PermissionMode from permission_enforcer.hpp

enum class PermissionOverride { Allow, Deny, Ask };

struct PermissionContext {
    std::optional<PermissionOverride> override_decision;
    std::optional<std::string>        override_reason;
};

struct PermissionRequest {
    std::string    tool_name;
    std::string    input;
    PermissionMode current_mode;
    PermissionMode required_mode;
    std::optional<std::string> reason;
};

enum class PermissionPromptDecision { Allow, Deny };
struct PermissionDeny { std::string reason; };

using PermissionOutcome = std::variant<std::monostate, PermissionDeny>;
inline bool is_permitted(const PermissionOutcome& o) { return std::holds_alternative<std::monostate>(o); }

// Prompter callback: returns true for allow, false for deny
using PermissionPrompter = std::function<bool(const PermissionRequest&)>;

class PermissionPolicy {
public:
    explicit PermissionPolicy(PermissionMode mode);

    PermissionPolicy& with_tool_requirement(const std::string& tool, PermissionMode required);
    PermissionPolicy& with_rules(const std::vector<std::string>& allow,
                                  const std::vector<std::string>& deny,
                                  const std::vector<std::string>& ask);

    PermissionMode active_mode() const { return active_mode_; }
    PermissionMode required_mode_for(const std::string& tool) const;

    PermissionOutcome authorize(const std::string& tool, const std::string& input,
                                PermissionPrompter prompter = nullptr) const;
    PermissionOutcome authorize_with_context(const std::string& tool, const std::string& input,
                                              const PermissionContext& ctx,
                                              PermissionPrompter prompter = nullptr) const;
private:
    struct Rule {
        std::string raw;
        std::string tool_name;
        enum class Matcher { Any, Exact, Prefix } matcher_type = Matcher::Any;
        std::string matcher_value;
        bool matches(const std::string& tool, const std::string& input) const;
    };

    static Rule parse_rule(const std::string& raw);
    static const Rule* find_match(const std::vector<Rule>& rules, const std::string& tool, const std::string& input);

    PermissionOutcome prompt_or_deny(const std::string& tool, const std::string& input,
                                      const std::optional<std::string>& reason,
                                      PermissionPrompter prompter) const;

    PermissionMode              active_mode_;
    std::map<std::string, PermissionMode> tool_requirements_;
    std::vector<Rule>           allow_rules_;
    std::vector<Rule>           deny_rules_;
    std::vector<Rule>           ask_rules_;
};

}  // namespace fliq
