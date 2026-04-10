#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::task_registry — Thread-safe sub-agent task lifecycle
// Translated from Rust runtime/task_registry.rs
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fliq {

enum class TaskStatus { Created, Running, Completed, Failed, Stopped };
const char* task_status_str(TaskStatus s);

struct TaskMessage {
    std::string role;
    std::string content;
    uint64_t    timestamp = 0;
};

struct Task {
    std::string              task_id;
    std::string              prompt;
    std::optional<std::string> description;
    TaskStatus               status = TaskStatus::Created;
    uint64_t                 created_at = 0;
    uint64_t                 updated_at = 0;
    std::vector<TaskMessage> messages;
    std::string              output;
    std::optional<std::string> team_id;
};

class TaskRegistry {
public:
    TaskRegistry() = default;

    Task                   create(const std::string& prompt, const std::optional<std::string>& desc = std::nullopt);
    std::optional<Task>    get(const std::string& task_id) const;
    std::vector<Task>      list(std::optional<TaskStatus> filter = std::nullopt) const;
    Task                   stop(const std::string& task_id);
    Task                   update(const std::string& task_id, const std::string& message);
    std::string            output(const std::string& task_id) const;
    void                   append_output(const std::string& task_id, const std::string& output);
    void                   set_status(const std::string& task_id, TaskStatus status);
    void                   assign_team(const std::string& task_id, const std::string& team_id);
    std::optional<Task>    remove(const std::string& task_id);
    size_t                 size() const;
    bool                   empty() const;

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, Task> tasks_;
    uint64_t counter_ = 0;
};

}  // namespace fliq
