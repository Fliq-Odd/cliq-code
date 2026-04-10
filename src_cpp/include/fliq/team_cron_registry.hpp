#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::team_cron_registry — Team and Cron lifecycle management
// Translated from Rust runtime/team_cron_registry.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fliq {

// ── Team ─────────────────────────────────────────────────────────────
enum class TeamStatus { Created, Running, Completed, Deleted };
const char* team_status_str(TeamStatus s);

struct Team {
    std::string              team_id;
    std::string              name;
    std::vector<std::string> task_ids;
    TeamStatus               status = TeamStatus::Created;
    uint64_t                 created_at = 0;
    uint64_t                 updated_at = 0;
};

class TeamRegistry {
public:
    TeamRegistry() = default;
    Team                create(const std::string& name, std::vector<std::string> task_ids);
    std::optional<Team> get(const std::string& team_id) const;
    std::vector<Team>   list() const;
    Team                del(const std::string& team_id);   // soft-delete
    std::optional<Team> remove(const std::string& team_id); // hard-delete
    size_t              size() const;
    bool                empty() const;
private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, Team> teams_;
    uint64_t counter_ = 0;
};

// ── Cron ─────────────────────────────────────────────────────────────
struct CronEntry {
    std::string              cron_id;
    std::string              schedule;
    std::string              prompt;
    std::optional<std::string> description;
    bool                     enabled = true;
    uint64_t                 created_at = 0;
    uint64_t                 updated_at = 0;
    std::optional<uint64_t>  last_run_at;
    uint64_t                 run_count = 0;
};

class CronRegistry {
public:
    CronRegistry() = default;
    CronEntry                create(const std::string& schedule, const std::string& prompt,
                                     const std::optional<std::string>& desc = std::nullopt);
    std::optional<CronEntry> get(const std::string& cron_id) const;
    std::vector<CronEntry>   list(bool enabled_only = false) const;
    CronEntry                del(const std::string& cron_id);
    void                     disable(const std::string& cron_id);
    void                     record_run(const std::string& cron_id);
    size_t                   size() const;
    bool                     empty() const;
private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, CronEntry> entries_;
    uint64_t counter_ = 0;
};

}  // namespace fliq
