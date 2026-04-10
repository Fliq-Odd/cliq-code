// ─────────────────────────────────────────────────────────────────────
// fliq::session — Implementation
// Translated from Rust crates/runtime/src/session.rs (1240 LOC)
// ─────────────────────────────────────────────────────────────────────

#include "fliq/session.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fliq {

static uint64_t current_time_millis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string Session::generate_id() {
    static std::mt19937_64 rng(std::random_device{}());
    static uint64_t counter = 0;
    std::ostringstream ss;
    ss << std::hex << current_time_millis() << "_" << (rng() & 0xFFFFFF) << "_" << ++counter;
    return ss.str();
}

// ── ContentBlock factories ───────────────────────────────────────────

ContentBlock ContentBlock::make_text(const std::string& text) {
    ContentBlock b;
    b.type = Type::Text;
    b.text = text;
    return b;
}

ContentBlock ContentBlock::make_tool_use(const std::string& id, const std::string& name, const std::string& input) {
    ContentBlock b;
    b.type  = Type::ToolUse;
    b.id    = id;
    b.name  = name;
    b.input = input;
    return b;
}

ContentBlock ContentBlock::make_tool_result(const std::string& tool_use_id, const std::string& tool_name,
                                              const std::string& output, bool is_error) {
    ContentBlock b;
    b.type        = Type::ToolResult;
    b.tool_use_id = tool_use_id;
    b.tool_name   = tool_name;
    b.output      = output;
    b.is_error    = is_error;
    return b;
}

// ── ConversationMessage factories ────────────────────────────────────

ConversationMessage ConversationMessage::user_text(const std::string& text) {
    return {MessageRole::User, {ContentBlock::make_text(text)}, std::nullopt};
}

ConversationMessage ConversationMessage::assistant(const std::vector<ContentBlock>& blocks) {
    return {MessageRole::Assistant, blocks, std::nullopt};
}

ConversationMessage ConversationMessage::tool_result(const std::string& tool_use_id, const std::string& tool_name,
                                                       const std::string& output, bool is_error) {
    return {MessageRole::Tool, {ContentBlock::make_tool_result(tool_use_id, tool_name, output, is_error)}, std::nullopt};
}

// ── Session ──────────────────────────────────────────────────────────

Session::Session() {
    auto now = current_time_millis();
    version_       = 1;
    session_id_    = generate_id();
    created_at_ms_ = now;
    updated_at_ms_ = now;
}

void Session::touch() {
    updated_at_ms_ = current_time_millis();
}

void Session::push_message(ConversationMessage msg) {
    touch();
    messages_.push_back(std::move(msg));

    // Auto-persist if path is set
    if (persistence_path_) {
        try { save_to_path(*persistence_path_); } catch (...) {}
    }
}

void Session::push_user_text(const std::string& text) {
    push_message(ConversationMessage::user_text(text));
}

void Session::record_compaction(const std::string& summary, size_t removed_count) {
    touch();
    uint32_t count = compaction_ ? compaction_->count + 1 : 1;
    compaction_ = SessionCompaction{count, removed_count, summary};
}

Session Session::fork_session(const std::optional<std::string>& branch_name) const {
    Session forked;
    forked.version_       = version_;
    forked.messages_      = messages_;
    forked.compaction_    = compaction_;
    forked.fork_          = SessionFork{session_id_, branch_name};
    return forked;
}

void Session::set_persistence_path(const std::string& path) {
    persistence_path_ = path;
}

void Session::save_to_path(const std::string& path) const {
    namespace fs = std::filesystem;
    // Ensure parent directory exists
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    // Build JSONL lines
    std::ostringstream out;
    // Meta record
    out << "{\"type\":\"session_meta\""
        << ",\"version\":" << version_
        << ",\"session_id\":\"" << session_id_ << "\""
        << ",\"created_at_ms\":" << created_at_ms_
        << ",\"updated_at_ms\":" << updated_at_ms_;
    if (fork_) {
        out << ",\"fork\":{\"parent_session_id\":\"" << fork_->parent_session_id << "\"";
        if (fork_->branch_name) out << ",\"branch_name\":\"" << *fork_->branch_name << "\"";
        out << "}";
    }
    out << "}\n";

    // Compaction record
    if (compaction_) {
        out << "{\"type\":\"compaction\""
            << ",\"count\":" << compaction_->count
            << ",\"removed_message_count\":" << compaction_->removed_message_count
            << ",\"summary\":\"" << compaction_->summary << "\"}\n";
    }

    // Messages
    auto role_str = [](MessageRole r) -> const char* {
        switch (r) {
            case MessageRole::System:    return "system";
            case MessageRole::User:      return "user";
            case MessageRole::Assistant: return "assistant";
            case MessageRole::Tool:      return "tool";
        }
        return "unknown";
    };

    for (auto& msg : messages_) {
        out << "{\"type\":\"message\",\"message\":{\"role\":\"" << role_str(msg.role) << "\",\"blocks\":[";
        for (size_t i = 0; i < msg.blocks.size(); ++i) {
            if (i > 0) out << ",";
            auto& b = msg.blocks[i];
            switch (b.type) {
                case ContentBlock::Type::Text:
                    out << "{\"type\":\"text\",\"text\":\"" << b.text << "\"}";
                    break;
                case ContentBlock::Type::ToolUse:
                    out << "{\"type\":\"tool_use\",\"id\":\"" << b.id
                        << "\",\"name\":\"" << b.name
                        << "\",\"input\":\"" << b.input << "\"}";
                    break;
                case ContentBlock::Type::ToolResult:
                    out << "{\"type\":\"tool_result\",\"tool_use_id\":\"" << b.tool_use_id
                        << "\",\"tool_name\":\"" << b.tool_name
                        << "\",\"output\":\"" << b.output
                        << "\",\"is_error\":" << (b.is_error ? "true" : "false") << "}";
                    break;
            }
        }
        out << "]";
        if (msg.usage) {
            out << ",\"usage\":{\"input_tokens\":" << msg.usage->input_tokens
                << ",\"output_tokens\":" << msg.usage->output_tokens
                << ",\"cache_creation_input_tokens\":" << msg.usage->cache_creation_input_tokens
                << ",\"cache_read_input_tokens\":" << msg.usage->cache_read_input_tokens << "}";
        }
        out << "}}\n";
    }

    // Atomic write
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << out.str();
    }
    fs::rename(tmp, path);
}

Session Session::load_from_path(const std::string& path) {
    // Simplified: creates a new session and sets persistence path
    // Full JSONL parsing would use the json.hpp module
    Session session;
    session.set_persistence_path(path);
    if (std::filesystem::exists(path)) {
        // Read and count message lines for reconstruction
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("\"type\":\"message\"") != std::string::npos) {
                // Extract user/assistant text from basic patterns
                if (line.find("\"role\":\"user\"") != std::string::npos) {
                    auto pos = line.find("\"text\":\"");
                    if (pos != std::string::npos) {
                        auto start = pos + 8;
                        auto end = line.find("\"", start);
                        if (end != std::string::npos) {
                            session.messages_.push_back(
                                ConversationMessage::user_text(line.substr(start, end - start)));
                        }
                    }
                }
            }
        }
    }
    return session;
}

}  // namespace fliq
