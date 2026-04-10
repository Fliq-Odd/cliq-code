#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::sse — Incremental Server-Sent Events parser
// Translated from Rust runtime/sse.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fliq {

struct SseEvent {
    std::optional<std::string> event;
    std::string                data;
    std::optional<std::string> id;
    std::optional<uint64_t>    retry;
};

class IncrementalSseParser {
public:
    IncrementalSseParser() = default;
    std::vector<SseEvent> push_chunk(const std::string& chunk);
    std::vector<SseEvent> finish();
private:
    void process_line(const std::string& line, std::vector<SseEvent>& out);
    std::optional<SseEvent> take_event();

    std::string                buffer_;
    std::optional<std::string> event_name_;
    std::vector<std::string>   data_lines_;
    std::optional<std::string> id_;
    std::optional<uint64_t>    retry_;
};

}  // namespace fliq
