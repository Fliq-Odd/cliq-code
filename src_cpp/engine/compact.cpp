// fliq::compact — Session compaction (Rust compact.rs 690 LOC)
#include "fliq/compact.hpp"
#include <algorithm>
#include <sstream>
#include <vector>

namespace fliq {

static const char* PREAMBLE = "This session is being continued from a previous conversation that ran out of context. The summary below covers the earlier portion of the conversation.\n\n";
static const char* RECENT_NOTE = "Recent messages are preserved verbatim.";
static const char* RESUME_INST = "Continue the conversation from where it left off without asking the user any further questions. Resume directly — do not acknowledge the summary, do not recap what was happening, and do not preface with continuation text.";

static std::string truncate(const std::string& s, size_t max) {
    if (s.size() <= max) return s;
    return s.substr(0, max) + "…";
}

static size_t estimate_block_tokens(const ContentBlock& b) {
    switch (b.type) {
        case ContentBlock::Type::Text: return b.text.size() / 4 + 1;
        case ContentBlock::Type::ToolUse: return (b.name.size() + b.input.size()) / 4 + 1;
        case ContentBlock::Type::ToolResult: return (b.tool_name.size() + b.output.size()) / 4 + 1;
    }
    return 1;
}

static size_t estimate_msg_tokens(const ConversationMessage& m) {
    size_t t = 0;
    for (auto& b : m.blocks) t += estimate_block_tokens(b);
    return t;
}

size_t estimate_session_tokens(const Session& s) {
    size_t t = 0;
    for (auto& m : s.messages()) t += estimate_msg_tokens(m);
    return t;
}

static std::string extract_tag(const std::string& c, const std::string& tag) {
    auto start = "<" + tag + ">";
    auto end = "</" + tag + ">";
    auto s = c.find(start);
    if (s == std::string::npos) return "";
    s += start.size();
    auto e = c.find(end, s);
    if (e == std::string::npos) return "";
    return c.substr(s, e - s);
}

static std::string strip_tag(const std::string& c, const std::string& tag) {
    auto start = "<" + tag + ">";
    auto end = "</" + tag + ">";
    auto s = c.find(start);
    auto e = c.find(end);
    if (s == std::string::npos || e == std::string::npos) return c;
    return c.substr(0, s) + c.substr(e + end.size());
}

std::string format_compact_summary(const std::string& summary) {
    auto without_analysis = strip_tag(summary, "analysis");
    auto content = extract_tag(without_analysis, "summary");
    if (!content.empty()) {
        auto tag_start = without_analysis.find("<summary>");
        auto tag_end = without_analysis.find("</summary>") + 10;
        std::string r = without_analysis.substr(0, tag_start);
        r += "Summary:\n";
        // Trim content
        while (!content.empty() && content.front() == '\n') content.erase(0, 1);
        while (!content.empty() && content.back() == '\n') content.pop_back();
        r += content;
        r += without_analysis.substr(tag_end);
        return r;
    }
    return without_analysis;
}

std::string get_compact_continuation_message(const std::string& summary, bool suppress, bool recent) {
    std::string base = std::string(PREAMBLE) + format_compact_summary(summary);
    if (recent) { base += "\n\n"; base += RECENT_NOTE; }
    if (suppress) { base += "\n"; base += RESUME_INST; }
    return base;
}

bool should_compact(const Session& s, CompactionConfig cfg) {
    // Check if first message is a compaction summary
    size_t start = 0;
    if (!s.messages().empty() && s.messages()[0].role == MessageRole::System) {
        for (auto& b : s.messages()[0].blocks) {
            if (b.type == ContentBlock::Type::Text && b.text.find(PREAMBLE) != std::string::npos) {
                start = 1; break;
            }
        }
    }
    auto compactable_count = s.messages().size() - start;
    if (compactable_count <= cfg.preserve_recent_messages) return false;

    size_t total = 0;
    for (size_t i = start; i < s.messages().size(); ++i)
        total += estimate_msg_tokens(s.messages()[i]);
    return total >= cfg.max_estimated_tokens;
}

CompactionResult compact_session(const Session& s, CompactionConfig cfg) {
    if (!should_compact(s, cfg))
        return {"", "", s, 0};

    size_t start = 0;
    if (!s.messages().empty() && s.messages()[0].role == MessageRole::System) {
        for (auto& b : s.messages()[0].blocks)
            if (b.type == ContentBlock::Type::Text && b.text.find(PREAMBLE) != std::string::npos)
                { start = 1; break; }
    }

    size_t keep_from = s.messages().size() > cfg.preserve_recent_messages
        ? s.messages().size() - cfg.preserve_recent_messages : start;

    // Build summary of removed messages
    size_t removed_count = keep_from - start;
    size_t user_count = 0, asst_count = 0, tool_count = 0;
    for (size_t i = start; i < keep_from; ++i) {
        switch (s.messages()[i].role) {
            case MessageRole::User: ++user_count; break;
            case MessageRole::Assistant: ++asst_count; break;
            case MessageRole::Tool: ++tool_count; break;
            default: break;
        }
    }

    std::ostringstream summary;
    summary << "<summary>\nConversation summary:\n"
            << "- Scope: " << removed_count << " earlier messages compacted "
            << "(user=" << user_count << ", assistant=" << asst_count << ", tool=" << tool_count << ").\n"
            << "- Key timeline:\n";
    for (size_t i = start; i < keep_from; ++i) {
        const char* role = "unknown";
        switch (s.messages()[i].role) {
            case MessageRole::System: role = "system"; break;
            case MessageRole::User: role = "user"; break;
            case MessageRole::Assistant: role = "assistant"; break;
            case MessageRole::Tool: role = "tool"; break;
        }
        std::string content;
        for (auto& b : s.messages()[i].blocks) {
            if (b.type == ContentBlock::Type::Text) content = truncate(b.text, 160);
        }
        summary << "  - " << role << ": " << content << "\n";
    }
    summary << "</summary>";

    auto sum_str = summary.str();
    auto formatted = format_compact_summary(sum_str);
    auto continuation = get_compact_continuation_message(sum_str, true, keep_from < s.messages().size());

    Session compacted;
    compacted.messages().push_back({MessageRole::System, {ContentBlock::make_text(continuation)}, std::nullopt});
    for (size_t i = keep_from; i < s.messages().size(); ++i)
        compacted.messages().push_back(s.messages()[i]);
    compacted.record_compaction(sum_str, removed_count);

    return {sum_str, formatted, compacted, removed_count};
}

}  // namespace fliq
