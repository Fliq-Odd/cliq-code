#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::compact — Session compaction (context window management)
// Translated from Rust runtime/compact.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstddef>
#include <string>

#include "fliq/session.hpp"

namespace fliq {

struct CompactionConfig {
    size_t preserve_recent_messages = 4;
    size_t max_estimated_tokens    = 10000;
};

struct CompactionResult {
    std::string summary;
    std::string formatted_summary;
    Session     compacted_session;
    size_t      removed_message_count = 0;
};

size_t           estimate_session_tokens(const Session& session);
bool             should_compact(const Session& session, CompactionConfig config);
std::string      format_compact_summary(const std::string& summary);
std::string      get_compact_continuation_message(const std::string& summary, bool suppress_follow_up, bool recent_preserved);
CompactionResult compact_session(const Session& session, CompactionConfig config);

}  // namespace fliq
