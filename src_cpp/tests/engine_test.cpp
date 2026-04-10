// ─────────────────────────────────────────────────────────────────────
// fliq C++ Engine — Standalone test runner
// Validates core engine functions without external test frameworks
// ─────────────────────────────────────────────────────────────────────

#include "fliq/file_ops.hpp"
#include "fliq/command_exec.hpp"
#include "fliq/permission_enforcer.hpp"
#include "fliq/directory_walker.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            std::cout << "  [RUN ] " << #name << std::endl; \
            try { \
                test_##name(); \
                std::cout << "  [PASS] " << #name << std::endl; \
                ++tests_passed; \
            } catch (const std::exception& e) { \
                std::cout << "  [FAIL] " << #name << ": " << e.what() << std::endl; \
                ++tests_failed; \
            } \
        } \
    } test_instance_##name; \
    void test_##name()

static std::string temp_dir() {
    auto p = fs::temp_directory_path() / "fliq_engine_tests";
    fs::create_directories(p);
    return p.string();
}

static std::string unique_path(const std::string& name) {
    static int counter = 0;
    auto p = fs::path(temp_dir()) / (name + "_" + std::to_string(++counter));
    return p.string();
}

// ═══════════════════════════════════════════════════════════════════
//  File Operations Tests
// ═══════════════════════════════════════════════════════════════════

TEST(read_and_write_file) {
    auto path = unique_path("rw_test.txt");
    auto result = fliq::write_file(path, "line one\nline two\nline three");
    assert(result.kind == "create");
    assert(result.content == "line one\nline two\nline three");

    auto read = fliq::read_file(path, 1, 1);
    assert(read.file.content == "line two");
    assert(read.file.num_lines == 1);

    fs::remove(path);
}

TEST(edit_file_replace_all) {
    auto path = unique_path("edit_test.txt");
    fliq::write_file(path, "alpha beta alpha");

    auto result = fliq::edit_file(path, "alpha", "omega", true);
    assert(result.replace_all == true);

    auto read = fliq::read_file(path);
    assert(read.file.content == "omega beta omega");

    fs::remove(path);
}

TEST(edit_file_replace_first) {
    auto path = unique_path("edit_first.txt");
    fliq::write_file(path, "alpha beta alpha");

    auto result = fliq::edit_file(path, "alpha", "omega", false);
    assert(result.replace_all == false);

    auto read = fliq::read_file(path);
    assert(read.file.content == "omega beta alpha");

    fs::remove(path);
}

TEST(rejects_binary_files) {
    auto path = unique_path("binary.bin");
    {
        std::ofstream f(path, std::ios::binary);
        char data[] = {'\x00', '\x01', '\x02', '\x03', 'b', 'i', 'n'};
        f.write(data, sizeof(data));
    }

    bool threw = false;
    try {
        fliq::read_file(path);
    } catch (const std::runtime_error& e) {
        threw = true;
        assert(std::string(e.what()).find("binary") != std::string::npos);
    }
    assert(threw);

    fs::remove(path);
}

TEST(rejects_oversized_writes) {
    auto path = unique_path("oversize.txt");
    std::string huge(fliq::MAX_WRITE_SIZE + 1, 'x');

    bool threw = false;
    try {
        fliq::write_file(path, huge);
    } catch (const std::runtime_error& e) {
        threw = true;
        assert(std::string(e.what()).find("too large") != std::string::npos);
    }
    assert(threw);
}

TEST(workspace_boundary_enforcement) {
    auto ws = unique_path("workspace");
    fs::create_directories(ws);
    auto inside = fs::path(ws) / "inside.txt";
    fliq::write_file(inside.string(), "safe content");

    // Reading inside should succeed
    auto read = fliq::read_file_in_workspace(inside.string(), {}, {}, ws);
    assert(read.file.content == "safe content");

    // Reading outside should fail
    auto outside = unique_path("outside.txt");
    fliq::write_file(outside, "unsafe");

    bool threw = false;
    try {
        fliq::read_file_in_workspace(outside, {}, {}, ws);
    } catch (const std::runtime_error& e) {
        threw = true;
        assert(std::string(e.what()).find("escapes") != std::string::npos);
    }
    assert(threw);

    fs::remove_all(ws);
    fs::remove(outside);
}

// ═══════════════════════════════════════════════════════════════════
//  Command Execution Tests
// ═══════════════════════════════════════════════════════════════════

TEST(execute_simple_command) {
#ifdef _WIN32
    fliq::CommandInput input;
    input.command = "echo hello";
    auto output = fliq::execute_command(input);
    assert(output.stdout_str.find("hello") != std::string::npos);
#else
    fliq::CommandInput input;
    input.command = "printf 'hello'";
    auto output = fliq::execute_command(input);
    assert(output.stdout_str.find("hello") != std::string::npos);
#endif
}

TEST(truncate_output_short) {
    auto result = fliq::truncate_output("hello world");
    assert(result == "hello world");
}

TEST(truncate_output_long) {
    std::string big(20000, 'x');
    auto result = fliq::truncate_output(big);
    assert(result.size() < 20000);
    assert(result.find("[output truncated") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════
//  Permission Enforcer Tests
// ═══════════════════════════════════════════════════════════════════

TEST(allow_mode_permits_everything) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::Allow);
    assert(fliq::is_allowed(enforcer.check("bash", "")));
    assert(fliq::is_allowed(enforcer.check("write_file", "")));
    assert(fliq::is_allowed(enforcer.check_file_write("/outside/path", "/workspace")));
    assert(fliq::is_allowed(enforcer.check_bash("rm -rf /")));
}

TEST(read_only_denies_writes) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::ReadOnly);
    assert(!fliq::is_allowed(enforcer.check("write_file", "")));
    assert(!fliq::is_allowed(enforcer.check_file_write("/workspace/file.rs", "/workspace")));
}

TEST(read_only_allows_read_commands) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::ReadOnly);
    assert(fliq::is_allowed(enforcer.check_bash("cat src/main.rs")));
    assert(fliq::is_allowed(enforcer.check_bash("ls -la")));
    assert(fliq::is_allowed(enforcer.check_bash("git log --oneline")));
}

TEST(read_only_denies_write_commands) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::ReadOnly);
    assert(!fliq::is_allowed(enforcer.check_bash("rm file.txt")));
}

TEST(workspace_write_denies_outside) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::WorkspaceWrite);
    assert(fliq::is_allowed(
        enforcer.check_file_write("/workspace/src/main.rs", "/workspace")));
    assert(!fliq::is_allowed(
        enforcer.check_file_write("/etc/passwd", "/workspace")));
}

TEST(prompt_mode_denies_bash_and_writes) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::Prompt);
    assert(!fliq::is_allowed(enforcer.check_bash("echo test")));
    assert(!fliq::is_allowed(
        enforcer.check_file_write("/workspace/file.rs", "/workspace")));
}

TEST(danger_full_access_permits_all) {
    fliq::PermissionEnforcer enforcer(fliq::PermissionMode::DangerFullAccess);
    assert(fliq::is_allowed(enforcer.check_file_write("/outside/file.txt", "/workspace")));
    assert(fliq::is_allowed(enforcer.check_bash("rm -rf /tmp/scratch")));
}

TEST(read_only_heuristic_basic) {
    assert(fliq::is_read_only_command("cat file.txt"));
    assert(fliq::is_read_only_command("grep pattern file"));
    assert(fliq::is_read_only_command("git log --oneline"));
    assert(!fliq::is_read_only_command("rm file.txt"));
    assert(!fliq::is_read_only_command("echo test > file.txt"));
    assert(!fliq::is_read_only_command("sed -i 's/a/b/' file"));
}

// ═══════════════════════════════════════════════════════════════════
//  Directory Walker Tests
// ═══════════════════════════════════════════════════════════════════

TEST(collect_files_recursive) {
    auto dir = unique_path("walker_test");
    fs::create_directories(fs::path(dir) / "sub");
    {
        std::ofstream(fs::path(dir) / "a.txt") << "hello";
        std::ofstream(fs::path(dir) / "sub" / "b.rs") << "fn main(){}";
    }

    auto all = fliq::collect_files(dir);
    assert(all.size() == 2);

    auto rust_only = fliq::collect_files_with_extension(dir, "rs");
    assert(rust_only.size() == 1);

    auto stats = fliq::get_directory_stats(dir);
    assert(stats.total_files == 2);
    assert(stats.total_size_bytes > 0);

    fs::remove_all(dir);
}

// ═══════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n══════════════════════════════════════════" << std::endl;
    std::cout << "  fliq C++ Engine — Test Results" << std::endl;
    std::cout << "══════════════════════════════════════════" << std::endl;
    std::cout << "  Passed: " << tests_passed << std::endl;
    std::cout << "  Failed: " << tests_failed << std::endl;
    std::cout << "══════════════════════════════════════════\n" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
