cmake_minimum_required(VERSION 3.15)
project(fliq_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Engine static library ─────────────────────────────────────────────
# All 11 modules translated from the Rust crates/runtime/src/
add_library(fliq_engine STATIC
    # Original 4 modules
    engine/file_ops.cpp
    engine/command_exec.cpp
    engine/permission_enforcer.cpp
    engine/directory_walker.cpp
    # Newly translated modules
    engine/bash_validation.cpp
    engine/permissions.cpp
    engine/session.cpp
    engine/compact.cpp
    engine/usage.cpp
    engine/sse.cpp
    engine/bootstrap.cpp
    engine/task_registry.cpp
    engine/team_cron_registry.cpp
    engine/sandbox.cpp
    engine/json.cpp
    engine/hooks.cpp
    engine/config.cpp
)
target_include_directories(fliq_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── Standalone test runner ────────────────────────────────────────────
add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE fliq_engine)

# ── pybind11 module (optional — only if pybind11 is found) ────────────
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(fliq_core bindings/pybind_module.cpp)
    target_link_libraries(fliq_core PRIVATE fliq_engine)
    message(STATUS "pybind11 found — building Python module 'fliq_core'")
else()
    message(STATUS "pybind11 not found — skipping Python bindings (install with: pip install pybind11)")
endif()
