// ─────────────────────────────────────────────────────────────────────
// fliq::task_registry — Implementation
// Translated from Rust crates/runtime/src/task_registry.rs
// ─────────────────────────────────────────────────────────────────────

#include "fliq/task_registry.hpp"

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

const char* task_status_str(TaskStatus s) {
    switch (s) {
        case TaskStatus::Created:   return "created";
        case TaskStatus::Running:   return "running";
        case TaskStatus::Completed: return "completed";
        case TaskStatus::Failed:    return "failed";
        case TaskStatus::Stopped:   return "stopped";
    }
    return "unknown";
}

Task TaskRegistry::create(const std::string& prompt, const std::optional<std::string>& desc) {
    std::lock_guard<std::mutex> lock(mtx_);
    ++counter_;
    auto ts = now_secs();
    std::ostringstream id;
    id << "task_" << std::hex << std::setfill('0') << std::setw(8) << ts << "_" << std::dec << counter_;

    Task task;
    task.task_id    = id.str();
    task.prompt     = prompt;
    task.description = desc;
    task.status     = TaskStatus::Created;
    task.created_at = ts;
    task.updated_at = ts;
    tasks_[task.task_id] = task;
    return task;
}

std::optional<Task> TaskRegistry::get(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    return it != tasks_.end() ? std::optional<Task>(it->second) : std::nullopt;
}

std::vector<Task> TaskRegistry::list(std::optional<TaskStatus> filter) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<Task> result;
    for (auto& [_, t] : tasks_)
        if (!filter || t.status == *filter) result.push_back(t);
    return result;
}

Task TaskRegistry::stop(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    auto& task = it->second;
    if (task.status == TaskStatus::Completed || task.status == TaskStatus::Failed || task.status == TaskStatus::Stopped)
        throw std::runtime_error("task " + task_id + " is already in terminal state: " + task_status_str(task.status));
    task.status = TaskStatus::Stopped;
    task.updated_at = now_secs();
    return task;
}

Task TaskRegistry::update(const std::string& task_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    auto& task = it->second;
    task.messages.push_back({"user", message, now_secs()});
    task.updated_at = now_secs();
    return task;
}

std::string TaskRegistry::output(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    return it->second.output;
}

void TaskRegistry::append_output(const std::string& task_id, const std::string& out) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    it->second.output += out;
    it->second.updated_at = now_secs();
}

void TaskRegistry::set_status(const std::string& task_id, TaskStatus status) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    it->second.status = status;
    it->second.updated_at = now_secs();
}

void TaskRegistry::assign_team(const std::string& task_id, const std::string& team_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) throw std::runtime_error("task not found: " + task_id);
    it->second.team_id = team_id;
    it->second.updated_at = now_secs();
}

std::optional<Task> TaskRegistry::remove(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) return std::nullopt;
    auto task = std::move(it->second);
    tasks_.erase(it);
    return task;
}

size_t TaskRegistry::size() const { std::lock_guard<std::mutex> lock(mtx_); return tasks_.size(); }
bool   TaskRegistry::empty() const { return size() == 0; }

}  // namespace fliq
