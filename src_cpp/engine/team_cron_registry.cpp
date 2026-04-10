// ─────────────────────────────────────────────────────────────────────
// fliq::team_cron_registry — Implementation
// Translated from Rust crates/runtime/src/team_cron_registry.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/team_cron_registry.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fliq {

static uint64_t now_secs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

const char* team_status_str(TeamStatus s) {
    switch (s) {
        case TeamStatus::Created:   return "created";
        case TeamStatus::Running:   return "running";
        case TeamStatus::Completed: return "completed";
        case TeamStatus::Deleted:   return "deleted";
    }
    return "unknown";
}

// ── TeamRegistry ─────────────────────────────────────────────────────

Team TeamRegistry::create(const std::string& name, std::vector<std::string> task_ids) {
    std::lock_guard<std::mutex> lock(mtx_);
    ++counter_;
    auto ts = now_secs();
    std::ostringstream id;
    id << "team_" << std::hex << std::setfill('0') << std::setw(8) << ts << "_" << std::dec << counter_;
    Team team;
    team.team_id   = id.str();
    team.name      = name;
    team.task_ids  = std::move(task_ids);
    team.status    = TeamStatus::Created;
    team.created_at = ts;
    team.updated_at = ts;
    teams_[team.team_id] = team;
    return team;
}

std::optional<Team> TeamRegistry::get(const std::string& team_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = teams_.find(team_id);
    return it != teams_.end() ? std::optional<Team>(it->second) : std::nullopt;
}

std::vector<Team> TeamRegistry::list() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<Team> result;
    for (auto& [_, t] : teams_) result.push_back(t);
    return result;
}

Team TeamRegistry::del(const std::string& team_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = teams_.find(team_id);
    if (it == teams_.end()) throw std::runtime_error("team not found: " + team_id);
    it->second.status = TeamStatus::Deleted;
    it->second.updated_at = now_secs();
    return it->second;
}

std::optional<Team> TeamRegistry::remove(const std::string& team_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = teams_.find(team_id);
    if (it == teams_.end()) return std::nullopt;
    auto team = std::move(it->second);
    teams_.erase(it);
    return team;
}

size_t TeamRegistry::size() const { std::lock_guard<std::mutex> lock(mtx_); return teams_.size(); }
bool   TeamRegistry::empty() const { return size() == 0; }

// ── CronRegistry ─────────────────────────────────────────────────────

CronEntry CronRegistry::create(const std::string& schedule, const std::string& prompt,
                                 const std::optional<std::string>& desc) {
    std::lock_guard<std::mutex> lock(mtx_);
    ++counter_;
    auto ts = now_secs();
    std::ostringstream id;
    id << "cron_" << std::hex << std::setfill('0') << std::setw(8) << ts << "_" << std::dec << counter_;
    CronEntry entry;
    entry.cron_id     = id.str();
    entry.schedule    = schedule;
    entry.prompt      = prompt;
    entry.description = desc;
    entry.enabled     = true;
    entry.created_at  = ts;
    entry.updated_at  = ts;
    entry.run_count   = 0;
    entries_[entry.cron_id] = entry;
    return entry;
}

std::optional<CronEntry> CronRegistry::get(const std::string& cron_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(cron_id);
    return it != entries_.end() ? std::optional<CronEntry>(it->second) : std::nullopt;
}

std::vector<CronEntry> CronRegistry::list(bool enabled_only) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<CronEntry> result;
    for (auto& [_, e] : entries_)
        if (!enabled_only || e.enabled) result.push_back(e);
    return result;
}

CronEntry CronRegistry::del(const std::string& cron_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(cron_id);
    if (it == entries_.end()) throw std::runtime_error("cron not found: " + cron_id);
    auto entry = std::move(it->second);
    entries_.erase(it);
    return entry;
}

void CronRegistry::disable(const std::string& cron_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(cron_id);
    if (it == entries_.end()) throw std::runtime_error("cron not found: " + cron_id);
    it->second.enabled = false;
    it->second.updated_at = now_secs();
}

void CronRegistry::record_run(const std::string& cron_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(cron_id);
    if (it == entries_.end()) throw std::runtime_error("cron not found: " + cron_id);
    it->second.last_run_at = now_secs();
    it->second.run_count++;
    it->second.updated_at = now_secs();
}

size_t CronRegistry::size() const { std::lock_guard<std::mutex> lock(mtx_); return entries_.size(); }
bool   CronRegistry::empty() const { return size() == 0; }

}  // namespace fliq
