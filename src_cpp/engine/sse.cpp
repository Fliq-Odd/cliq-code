// ─────────────────────────────────────────────────────────────────────
// fliq::sse — Implementation
// Translated from Rust crates/runtime/src/sse.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/sse.hpp"
#include <sstream>
#include <algorithm>

namespace fliq {

std::vector<SseEvent> IncrementalSseParser::push_chunk(const std::string& chunk) {
    buffer_ += chunk;
    std::vector<SseEvent> events;

    size_t pos;
    while ((pos = buffer_.find('\n')) != std::string::npos) {
        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 1);
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        process_line(line, events);
    }
    return events;
}

std::vector<SseEvent> IncrementalSseParser::finish() {
    std::vector<SseEvent> events;
    if (!buffer_.empty()) {
        std::string line = std::move(buffer_);
        buffer_.clear();
        if (!line.empty() && line.back() == '\r') line.pop_back();
        process_line(line, events);
    }
    if (auto ev = take_event()) events.push_back(std::move(*ev));
    return events;
}

void IncrementalSseParser::process_line(const std::string& line, std::vector<SseEvent>& out) {
    if (line.empty()) {
        if (auto ev = take_event()) out.push_back(std::move(*ev));
        return;
    }
    if (line[0] == ':') return;  // Comment

    std::string field, value;
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        field = line.substr(0, colon);
        value = line.substr(colon + 1);
        if (!value.empty() && value[0] == ' ') value = value.substr(1);
    } else {
        field = line;
    }

    if (field == "event")      event_name_ = value;
    else if (field == "data")  data_lines_.push_back(value);
    else if (field == "id")    id_ = value;
    else if (field == "retry") {
        try { retry_ = std::stoull(value); } catch (...) {}
    }
}

std::optional<SseEvent> IncrementalSseParser::take_event() {
    if (data_lines_.empty() && !event_name_ && !id_ && !retry_) return std::nullopt;

    std::string data;
    for (size_t i = 0; i < data_lines_.size(); ++i) {
        if (i > 0) data += '\n';
        data += data_lines_[i];
    }
    data_lines_.clear();

    SseEvent ev;
    ev.event = std::move(event_name_);
    ev.data  = std::move(data);
    ev.id    = std::move(id_);
    ev.retry = retry_;
    event_name_.reset(); id_.reset(); retry_.reset();
    return ev;
}

}  // namespace fliq
