#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::session — Conversation session persistence (JSON/JSONL)
// Translated from Rust runtime/session.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fliq/usage.hpp"

namespace fliq {

enum class MessageRole { System, User, Assistant, Tool };

struct ContentBlock {
    enum class Type { Text, ToolUse, ToolResult };
    Type type;
    // Text
    std::string text;
    // ToolUse
    std::string id, name, input;
    // ToolResult
    std::string tool_use_id, tool_name, output;
    bool        is_error = false;

    static ContentBlock make_text(const std::string& text);
    static ContentBlock make_tool_use(const std::string& id, const std::string& name, const std::string& input);
    static ContentBlock make_tool_result(const std::string& tool_use_id, const std::string& tool_name,
                                          const std::string& output, bool is_error);
};

struct ConversationMessage {
    MessageRole              role;
    std::vector<ContentBlock> blocks;
    std::optional<TokenUsage> usage;

    static ConversationMessage user_text(const std::string& text);
    static ConversationMessage assistant(const std::vector<ContentBlock>& blocks);
    static ConversationMessage tool_result(const std::string& tool_use_id, const std::string& tool_name,
                                            const std::string& output, bool is_error);
};

struct SessionCompaction {
    uint32_t    count = 0;
    size_t      removed_message_count = 0;
    std::string summary;
};

struct SessionFork {
    std::string               parent_session_id;
    std::optional<std::string> branch_name;
};

class Session {
public:
    Session();

    // Core accessors
    const std::string&  session_id()    const { return session_id_; }
    uint64_t            created_at_ms() const { return created_at_ms_; }
    uint64_t            updated_at_ms() const { return updated_at_ms_; }
    uint32_t            version()       const { return version_; }
    const std::vector<ConversationMessage>& messages() const { return messages_; }
    std::vector<ConversationMessage>&       messages()       { return messages_; }
    const std::optional<SessionCompaction>& compaction() const { return compaction_; }
    const std::optional<SessionFork>&       fork_info()  const { return fork_; }

    // Mutation
    void push_message(ConversationMessage msg);
    void push_user_text(const std::string& text);
    void record_compaction(const std::string& summary, size_t removed_count);
    Session fork_session(const std::optional<std::string>& branch_name = std::nullopt) const;

    // Persistence
    void save_to_path(const std::string& path) const;
    static Session load_from_path(const std::string& path);
    void set_persistence_path(const std::string& path);

private:
    void touch();
    static std::string generate_id();

    uint32_t version_ = 1;
    std::string session_id_;
    uint64_t created_at_ms_ = 0;
    uint64_t updated_at_ms_ = 0;
    std::vector<ConversationMessage> messages_;
    std::optional<SessionCompaction> compaction_;
    std::optional<SessionFork>       fork_;
    std::optional<std::string>       persistence_path_;
};

}  // namespace fliq
