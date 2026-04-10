// ─────────────────────────────────────────────────────────────────────
// pybind11 bindings — Exposes the C++ engine to Python as `fliq_core`
// Phase 3: The Bridge
// ─────────────────────────────────────────────────────────────────────

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "fliq/file_ops.hpp"
#include "fliq/command_exec.hpp"
#include "fliq/permission_enforcer.hpp"
#include "fliq/directory_walker.hpp"

namespace py = pybind11;

PYBIND11_MODULE(fliq_core, m) {
    m.doc() = "fliq C++ engine — high-performance file I/O, command execution, "
              "permission enforcement, and directory traversal";

    // ── File Operations ──────────────────────────────────────────────

    py::class_<fliq::TextFilePayload>(m, "TextFilePayload")
        .def_readonly("file_path",   &fliq::TextFilePayload::file_path)
        .def_readonly("content",     &fliq::TextFilePayload::content)
        .def_readonly("num_lines",   &fliq::TextFilePayload::num_lines)
        .def_readonly("start_line",  &fliq::TextFilePayload::start_line)
        .def_readonly("total_lines", &fliq::TextFilePayload::total_lines);

    py::class_<fliq::ReadFileOutput>(m, "ReadFileOutput")
        .def_readonly("kind", &fliq::ReadFileOutput::kind)
        .def_readonly("file", &fliq::ReadFileOutput::file);

    py::class_<fliq::WriteFileOutput>(m, "WriteFileOutput")
        .def_readonly("kind",          &fliq::WriteFileOutput::kind)
        .def_readonly("file_path",     &fliq::WriteFileOutput::file_path)
        .def_readonly("content",       &fliq::WriteFileOutput::content)
        .def_readonly("original_file", &fliq::WriteFileOutput::original_file);

    py::class_<fliq::EditFileOutput>(m, "EditFileOutput")
        .def_readonly("file_path",     &fliq::EditFileOutput::file_path)
        .def_readonly("old_string",    &fliq::EditFileOutput::old_string)
        .def_readonly("new_string",    &fliq::EditFileOutput::new_string)
        .def_readonly("original_file", &fliq::EditFileOutput::original_file)
        .def_readonly("replace_all",   &fliq::EditFileOutput::replace_all);

    py::class_<fliq::GlobSearchOutput>(m, "GlobSearchOutput")
        .def_readonly("duration_ms", &fliq::GlobSearchOutput::duration_ms)
        .def_readonly("num_files",   &fliq::GlobSearchOutput::num_files)
        .def_readonly("filenames",   &fliq::GlobSearchOutput::filenames)
        .def_readonly("truncated",   &fliq::GlobSearchOutput::truncated);

    py::class_<fliq::GrepSearchInput>(m, "GrepSearchInput")
        .def(py::init<>())
        .def_readwrite("pattern",          &fliq::GrepSearchInput::pattern)
        .def_readwrite("path",             &fliq::GrepSearchInput::path)
        .def_readwrite("glob",             &fliq::GrepSearchInput::glob)
        .def_readwrite("output_mode",      &fliq::GrepSearchInput::output_mode)
        .def_readwrite("before",           &fliq::GrepSearchInput::before)
        .def_readwrite("after",            &fliq::GrepSearchInput::after)
        .def_readwrite("context",          &fliq::GrepSearchInput::context)
        .def_readwrite("line_numbers",     &fliq::GrepSearchInput::line_numbers)
        .def_readwrite("case_insensitive", &fliq::GrepSearchInput::case_insensitive)
        .def_readwrite("file_type",        &fliq::GrepSearchInput::file_type)
        .def_readwrite("head_limit",       &fliq::GrepSearchInput::head_limit)
        .def_readwrite("offset",           &fliq::GrepSearchInput::offset);

    py::class_<fliq::GrepSearchOutput>(m, "GrepSearchOutput")
        .def_readonly("mode",           &fliq::GrepSearchOutput::mode)
        .def_readonly("num_files",      &fliq::GrepSearchOutput::num_files)
        .def_readonly("filenames",      &fliq::GrepSearchOutput::filenames)
        .def_readonly("content",        &fliq::GrepSearchOutput::content)
        .def_readonly("num_lines",      &fliq::GrepSearchOutput::num_lines)
        .def_readonly("num_matches",    &fliq::GrepSearchOutput::num_matches)
        .def_readonly("applied_limit",  &fliq::GrepSearchOutput::applied_limit)
        .def_readonly("applied_offset", &fliq::GrepSearchOutput::applied_offset);

    m.def("read_file", &fliq::read_file,
          py::arg("path"),
          py::arg("offset") = py::none(),
          py::arg("limit")  = py::none(),
          "Read a file with optional line-based pagination");

    m.def("write_file", &fliq::write_file,
          py::arg("path"), py::arg("content"),
          "Write content to a file");

    m.def("edit_file", &fliq::edit_file,
          py::arg("path"), py::arg("old_string"), py::arg("new_string"),
          py::arg("replace_all") = false,
          "Edit a file by replacing text");

    m.def("glob_search", &fliq::glob_search,
          py::arg("pattern"), py::arg("base_path") = py::none(),
          "Search for files matching a glob pattern");

    m.def("grep_search", &fliq::grep_search,
          py::arg("input"),
          "Regex search across files");

    m.def("is_binary_file", &fliq::is_binary_file,
          py::arg("path"), "Check if a file is binary");

    m.def("read_file_in_workspace", &fliq::read_file_in_workspace,
          py::arg("path"), py::arg("offset"), py::arg("limit"),
          py::arg("workspace_root"),
          "Read a file with workspace boundary enforcement");

    m.def("write_file_in_workspace", &fliq::write_file_in_workspace,
          py::arg("path"), py::arg("content"), py::arg("workspace_root"),
          "Write a file with workspace boundary enforcement");

    // ── Command Execution ────────────────────────────────────────────

    py::class_<fliq::CommandInput>(m, "CommandInput")
        .def(py::init<>())
        .def_readwrite("command",           &fliq::CommandInput::command)
        .def_readwrite("timeout_ms",        &fliq::CommandInput::timeout_ms)
        .def_readwrite("description",       &fliq::CommandInput::description)
        .def_readwrite("run_in_background", &fliq::CommandInput::run_in_background)
        .def_readwrite("working_directory", &fliq::CommandInput::working_directory);

    py::class_<fliq::CommandOutput>(m, "CommandOutput")
        .def_readonly("stdout_str",                &fliq::CommandOutput::stdout_str)
        .def_readonly("stderr_str",                &fliq::CommandOutput::stderr_str)
        .def_readonly("interrupted",               &fliq::CommandOutput::interrupted)
        .def_readonly("exit_code",                 &fliq::CommandOutput::exit_code)
        .def_readonly("background_task_id",        &fliq::CommandOutput::background_task_id)
        .def_readonly("return_code_interpretation", &fliq::CommandOutput::return_code_interpretation)
        .def_readonly("no_output_expected",        &fliq::CommandOutput::no_output_expected);

    m.def("execute_command", &fliq::execute_command,
          py::arg("input"), "Execute a shell command");

    m.def("truncate_output", &fliq::truncate_output,
          py::arg("s"), "Truncate output to 16 KiB");

    // ── Permission Enforcer ──────────────────────────────────────────

    py::enum_<fliq::PermissionMode>(m, "PermissionMode")
        .value("ReadOnly",         fliq::PermissionMode::ReadOnly)
        .value("WorkspaceWrite",   fliq::PermissionMode::WorkspaceWrite)
        .value("Prompt",           fliq::PermissionMode::Prompt)
        .value("Allow",            fliq::PermissionMode::Allow)
        .value("DangerFullAccess", fliq::PermissionMode::DangerFullAccess);

    py::class_<fliq::PermissionEnforcer>(m, "PermissionEnforcer")
        .def(py::init<fliq::PermissionMode>())
        .def("active_mode", &fliq::PermissionEnforcer::active_mode);

    m.def("is_read_only_command", &fliq::is_read_only_command,
          py::arg("command"), "Check if a bash command is read-only");

    m.def("is_within_workspace", &fliq::is_within_workspace,
          py::arg("path"), py::arg("workspace_root"),
          "Check if path is within the workspace");

    // ── Directory Walker ─────────────────────────────────────────────

    py::class_<fliq::DirectoryStats>(m, "DirectoryStats")
        .def_readonly("total_files",      &fliq::DirectoryStats::total_files)
        .def_readonly("total_size_bytes", &fliq::DirectoryStats::total_size_bytes);

    m.def("collect_files", &fliq::collect_files,
          py::arg("root_dir"), "Recursively collect all files");

    m.def("collect_files_with_extension", &fliq::collect_files_with_extension,
          py::arg("root_dir"), py::arg("extension"),
          "Collect files with a specific extension");

    m.def("get_directory_stats", &fliq::get_directory_stats,
          py::arg("root_dir"), "Get total files and size stats");
}
