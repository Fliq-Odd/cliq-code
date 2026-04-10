// fliq::permissions — Rule-based authorization (Rust permissions.rs 676 LOC)
#include "fliq/permissions.hpp"
#include "fliq/permission_enforcer.hpp"
#include <algorithm>
#include <sstream>

namespace fliq {

// ── Rule parsing helpers ─────────────────────────────────────────────
static size_t find_first_unescaped(const std::string& s, char needle) {
    bool esc = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\') { esc = !esc; continue; }
        if (s[i] == needle && !esc) return i;
        esc = false;
    }
    return std::string::npos;
}

static size_t find_last_unescaped(const std::string& s, char needle) {
    for (size_t i = s.size(); i-- > 0; ) {
        if (s[i] != needle) continue;
        size_t bs = 0;
        for (size_t j = i; j-- > 0 && s[j] == '\\'; ) ++bs;
        if (bs % 2 == 0) return i;
    }
    return std::string::npos;
}

static std::string unescape(const std::string& c) {
    std::string r = c;
    size_t p;
    while ((p = r.find("\\(")) != std::string::npos) r.replace(p, 2, "(");
    while ((p = r.find("\\)")) != std::string::npos) r.replace(p, 2, ")");
    while ((p = r.find("\\\\")) != std::string::npos) r.replace(p, 2, "\\");
    return r;
}

static std::optional<std::string> extract_subject(const std::string& input) {
    // Try JSON key extraction for common keys
    for (auto& key : {"command","path","file_path","filePath","url","pattern","code","message"}) {
        std::string search = "\"" + std::string(key) + "\":\"";
        auto pos = input.find(search);
        if (pos != std::string::npos) {
            auto start = pos + search.size();
            auto end = input.find("\"", start);
            if (end != std::string::npos) return input.substr(start, end - start);
        }
    }
    if (!input.empty()) return input;
    return std::nullopt;
}

// ── PermissionPolicy::Rule ───────────────────────────────────────────
bool PermissionPolicy::Rule::matches(const std::string& tool, const std::string& input) const {
    if (tool_name != tool) return false;
    switch (matcher_type) {
        case Matcher::Any: return true;
        case Matcher::Exact: {
            auto subj = extract_subject(input);
            return subj && *subj == matcher_value;
        }
        case Matcher::Prefix: {
            auto subj = extract_subject(input);
            return subj && subj->rfind(matcher_value, 0) == 0;
        }
    }
    return false;
}

PermissionPolicy::Rule PermissionPolicy::parse_rule(const std::string& raw) {
    std::string trimmed = raw;
    while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(0, 1);
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();

    auto open = find_first_unescaped(trimmed, '(');
    auto close = find_last_unescaped(trimmed, ')');

    if (open != std::string::npos && close != std::string::npos &&
        close == trimmed.size() - 1 && open < close) {
        auto tool = trimmed.substr(0, open);
        while (!tool.empty() && tool.back() == ' ') tool.pop_back();
        auto content = trimmed.substr(open + 1, close - open - 1);
        if (!tool.empty()) {
            auto unesc = unescape(content);
            while (!unesc.empty() && unesc.front() == ' ') unesc.erase(0, 1);
            while (!unesc.empty() && unesc.back() == ' ') unesc.pop_back();

            Rule r;
            r.raw = trimmed;
            r.tool_name = tool;
            if (unesc.empty() || unesc == "*") {
                r.matcher_type = Rule::Matcher::Any;
            } else if (unesc.size() > 2 && unesc.substr(unesc.size()-2) == ":*") {
                r.matcher_type = Rule::Matcher::Prefix;
                r.matcher_value = unesc.substr(0, unesc.size()-2);
            } else {
                r.matcher_type = Rule::Matcher::Exact;
                r.matcher_value = unesc;
            }
            return r;
        }
    }

    return {trimmed, trimmed, Rule::Matcher::Any, ""};
}

const PermissionPolicy::Rule* PermissionPolicy::find_match(
    const std::vector<Rule>& rules, const std::string& tool, const std::string& input) {
    for (auto& r : rules) if (r.matches(tool, input)) return &r;
    return nullptr;
}

// ── PermissionPolicy ─────────────────────────────────────────────────
PermissionPolicy::PermissionPolicy(PermissionMode mode) : active_mode_(mode) {}

PermissionPolicy& PermissionPolicy::with_tool_requirement(const std::string& tool, PermissionMode req) {
    tool_requirements_[tool] = req;
    return *this;
}

PermissionPolicy& PermissionPolicy::with_rules(const std::vector<std::string>& allow,
                                                 const std::vector<std::string>& deny,
                                                 const std::vector<std::string>& ask) {
    for (auto& r : allow) allow_rules_.push_back(parse_rule(r));
    for (auto& r : deny) deny_rules_.push_back(parse_rule(r));
    for (auto& r : ask) ask_rules_.push_back(parse_rule(r));
    return *this;
}

PermissionMode PermissionPolicy::required_mode_for(const std::string& tool) const {
    auto it = tool_requirements_.find(tool);
    return it != tool_requirements_.end() ? it->second : PermissionMode::DangerFullAccess;
}

PermissionOutcome PermissionPolicy::authorize(const std::string& tool, const std::string& input,
                                                PermissionPrompter prompter) const {
    return authorize_with_context(tool, input, {}, std::move(prompter));
}

PermissionOutcome PermissionPolicy::authorize_with_context(
    const std::string& tool, const std::string& input,
    const PermissionContext& ctx, PermissionPrompter prompter) const {

    // Deny rules short-circuit
    if (auto* rule = find_match(deny_rules_, tool, input))
        return PermissionDeny{"Permission to use " + tool + " denied by rule '" + rule->raw + "'"};

    auto required = required_mode_for(tool);
    auto* ask_rule = find_match(ask_rules_, tool, input);
    auto* allow_rule = find_match(allow_rules_, tool, input);

    // Context override
    if (ctx.override_decision) {
        if (*ctx.override_decision == PermissionOverride::Deny)
            return PermissionDeny{ctx.override_reason.value_or("denied by hook")};
        if (*ctx.override_decision == PermissionOverride::Ask)
            return prompt_or_deny(tool, input, ctx.override_reason, prompter);
        // Allow override
        if (ask_rule)
            return prompt_or_deny(tool, input, {"ask rule '" + ask_rule->raw + "'"}, prompter);
        if (allow_rule || active_mode_ == PermissionMode::Allow || active_mode_ >= required)
            return std::monostate{};
    }

    if (ask_rule) return prompt_or_deny(tool, input, {"ask rule '" + ask_rule->raw + "'"}, prompter);
    if (allow_rule || active_mode_ == PermissionMode::Allow || active_mode_ >= required)
        return std::monostate{};

    if (active_mode_ == PermissionMode::Prompt ||
        (active_mode_ == PermissionMode::WorkspaceWrite && required == PermissionMode::DangerFullAccess))
        return prompt_or_deny(tool, input, {tool + " requires escalation"}, prompter);

    return PermissionDeny{tool + " requires " + permission_mode_str(required) + "; current is " + permission_mode_str(active_mode_)};
}

PermissionOutcome PermissionPolicy::prompt_or_deny(
    const std::string& tool, const std::string& input,
    const std::optional<std::string>& reason, PermissionPrompter prompter) const {
    if (!prompter)
        return PermissionDeny{reason.value_or(tool + " requires approval")};

    PermissionRequest req{tool, input, active_mode_, required_mode_for(tool), reason};
    if (prompter(req)) return std::monostate{};
    return PermissionDeny{"User denied"};
}

}  // namespace fliq
