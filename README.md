# CLIQ Code (by fliq odd)

> **A premium, model-agnostic AI coding assistant** with a high-performance C++ engine and a Python-powered LLM brain, developed by **fliq odd**. Use any LLM provider (Gemini, OpenAI, Anthropic, Llama, Ollama) right from your terminal.

---

## What Is This?

CLIQ Code is a **command-line AI assistant** that can:
- **Read, write, and edit files** on your computer
- **Run shell commands** (with safety checks before anything destructive)
- **Search & navigate** your codebase (grep, glob, directory traversal)
- **Use any LLM** -- swap models by just changing an API key
- **Permission system** -- 5 security modes from read-only to full-access
- **Session memory** -- remembers your conversation, auto-compacts when context is full

### How It Works (Architecture)

```
+----------------------------------------------+
|              YOUR TERMINAL                    |
|                                              |
|  +-----------------------------------------+ |
|  |          Python Frontend                | |
|  |  - Rich terminal UI (colors, markdown)  | |
|  |  - LiteLLM brain (any LLM provider)     | |
|  |  - Safety Catch (blocks dangerous cmds) | |
|  |  - REPL + slash commands                | |
|  +------------------+-----------------------+ |
|                     | pybind11 bridge         |
|  +------------------v-----------------------+ |
|  |          C++ Engine (17 modules)         | |
|  |  - File I/O with boundary enforcement   | |
|  |  - Command execution (Win32/POSIX)      | |
|  |  - Permission policy engine             | |
|  |  - Bash command validation pipeline     | |
|  |  - Session persistence (JSONL)          | |
|  |  - Auto-compaction (context management) | |
|  |  - Task & Team registries               | |
|  |  - Hook system (pre/post tool-use)      | |
|  |  - SSE parser (streaming responses)     | |
|  |  - Sandbox detection (Docker/K8s)       | |
|  +-----------------------------------------+ |
+----------------------------------------------+
```

---

## Prerequisites (What You Need Installed)

### Required

| Software | Version | What It's For | How to Install |
|----------|---------|---------------|----------------|
| **Python** | 3.10+ | Runs the AI brain & UI | [python.org/downloads](https://www.python.org/downloads/) |
| **pip** | Latest | Installs Python packages | Comes with Python |
| **CMake** | 3.15+ | Builds the C++ engine (optional) | [cmake.org/download](https://cmake.org/download/) |
| **C++ Compiler** | C++20 support | Compiles the engine (optional) | See below |

### C++ Compiler (pick one for your OS) -- OPTIONAL

| OS | Compiler | How to Install |
|----|----------|----------------|
| **Windows** | MSVC (Visual Studio) | Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload |
| **Windows** | MinGW-w64 | `winget install -e --id MSYS2.MSYS2` then `pacman -S mingw-w64-ucrt-x86_64-gcc` |
| **macOS** | Clang (Xcode) | `xcode-select --install` |
| **Linux** | GCC | `sudo apt install build-essential cmake` (Ubuntu/Debian) |

### API Keys (pick at least one)

You need an API key from at least one LLM provider:

| Provider | Get a Key | Environment Variable |
|----------|-----------|---------------------|
| **Google Gemini** (recommended for testing) | [aistudio.google.com](https://aistudio.google.com/apikey) | `GEMINI_API_KEY` |
| **OpenAI** | [platform.openai.com](https://platform.openai.com/api-keys) | `OPENAI_API_KEY` |
| **Anthropic** | [console.anthropic.com](https://console.anthropic.com/) | `ANTHROPIC_API_KEY` |

---

## Installation Options

Choose the installation path that best fits your needs:

### Path A: Standalone Executable (Easiest — No Python Required)
Ideal for end-users who want to run CLIQ Code immediately without installing Python, Git, or compilers.

> **Note:** The standalone Windows executable is signed by the [SignPath Foundation](https://signpath.org/).

1. Go to the [Releases](https://github.com/fliq-odd/cliq-code/releases) page.
2. Download the pre-compiled `cliq-code.exe` binary.
3. Move the binary to a folder of your choice (e.g., `C:\Program Files\cliq-code`) and add that folder to your system's `PATH` variable to run it from any directory.

---

### Path B: Install via pip (Python Users)
If you already have Python 3.10+ installed and want to run it as a global package without cloning the repository:
```bash
pip install git+https://github.com/fliq-odd/cliq-code.git
```
This installs the tool globally along with all core dependencies (`litellm`, `rich`, `InquirerPy`). You can run it anywhere on your machine by simply typing:
```bash
cliq-code
```

---

### Path C: Developer Dev Setup (Clone & Compile C++ Engine)
For developers who want to modify CLIQ Code or compile the high-performance C++ helper engine:

1. **Clone the repository:**
   ```bash
   git clone https://github.com/fliq-odd/cliq-code.git
   cd cliq-code
   ```
2. **Install in editable mode with development dependencies:**
   ```bash
   pip install -e .[dev]
   ```
3. **Build the C++ Engine (Optional):**
   Ensure you have CMake and a C++20 compiler installed. Run:
   ```bash
   cmake -B build -S src_cpp
   cmake --build build --config Release
   ```
   Verify the build by running the C++ test suite:
   * **Windows:** `.\build\Release\engine_test.exe`
   * **macOS/Linux:** `./build/engine_test`

---

## Configuration & API Keys

Before launching CLIQ Code, you must set an API key for your chosen AI model provider. We recommend starting with Google Gemini as it offers a fast, free tier for testing.

### 1. Set Your API Key

#### Windows (PowerShell):
```powershell
# Temporarily for the current session:
$env:GEMINI_API_KEY = "your-api-key-here"

# Permanently (survives system restarts — run once):
[System.Environment]::SetEnvironmentVariable("GEMINI_API_KEY", "your-api-key-here", "User")
```

#### macOS/Linux (bash/zsh):
```bash
# Append to ~/.bashrc or ~/.zshrc:
export GEMINI_API_KEY="your-api-key-here"
# Reload terminal configuration:
source ~/.bashrc
```

*Note: You can swap Gemini for OpenAI (`OPENAI_API_KEY`) or Anthropic (`ANTHROPIC_API_KEY`) using the same syntax.*

### 2. Start CLIQ Code
Once your path is set up and key is configured, start the interactive REPL:
```bash
# If installed via Path A / Path B:
cliq-code

# If running directly from Path C source code:
python -m src_python.main
```

---

## How to Use CLIQ Code

### Interactive Mode (REPL)

When you run `cliq-code`, you get an interactive session:

```
fliq v0.1.0 -- Lightning-fast AI coding agent
Model: gemini/gemini-2.0-flash
Workspace: C:\path\to\fliq
Safety Catch: ACTIVE
Type /help for commands, /quit to exit

> help me refactor the login function in src/auth.py
```

Just type your request in natural language. CLIQ Code will:
1. Understand what you need
2. Read your files
3. Make edits or run commands
4. Ask for permission before anything destructive

### Slash Commands

| Command | What It Does |
|---------|-------------|
| `/help` | Show all available commands |
| `/model gemini/gemini-2.0-flash` | Switch to a different LLM model |
| `/model openai/gpt-4o` | Switch to GPT-4o |
| `/model anthropic/claude-sonnet-4-20250514` | Switch to Claude |
| `/safety` | Toggle the Safety Catch on/off |
| `/config` | Show current configuration |
| `/context` | Show conversation context size |
| `/tokens` | Show token usage & estimated cost |
| `/clear` | Clear conversation history |
| `/quit` or `Ctrl+C` | Exit CLIQ Code |

---

## Safety System

CLIQ Code has a **multi-layered safety system** to prevent accidental damage:

### Permission Modes

| Mode | Level | What It Allows |
|------|-------|---------------|
| `read-only` | Safest | Only read operations (ls, cat, grep) |
| `workspace-write` | Default | Read + write files inside your project folder |
| `prompt` | Interactive | Asks you before every write operation |
| `allow` | Trusting | Allows all operations without asking |
| `danger-full-access` | Full access | Unrestricted (never use this casually) |

### Safety Catch

Before running any shell command, CLIQ Code checks:
1. **Read-only validation** -- blocks `rm`, `mv`, etc. in read-only mode
2. **Destructive detection** -- warns about `rm -rf /`, fork bombs, etc.
3. **Path validation** -- catches `../` traversal and `~/` escapes
4. **Sed validation** -- blocks `sed -i` in read-only mode
5. **User confirmation** -- shows you the exact command and asks Y/N

---

## Configuration

CLIQ Code looks for config files in this order (later files override earlier ones):

| Priority | Location | Scope |
|----------|----------|-------|
| 1 | `~/.fliq/settings.json` | User-level (all projects) |
| 2 | `.fliq.json` (in project root) | Project-level |
| 3 | `.fliq/settings.json` | Project-level |
| 4 | `.fliq/settings.local.json` | Local overrides (gitignored) |

### Example Config File

Create `~/.fliq/settings.json`:
```json
{
    "model": "gemini/gemini-2.0-flash",
    "permissionMode": "workspace-write",
    "permissions": {
        "allow": ["Read(*)", "Grep(*)"],
        "deny": ["Bash(rm -rf *)"],
        "ask": ["Bash(*)"]
    },
    "hooks": {
        "PreToolUse": [],
        "PostToolUse": [],
        "PostToolUseFailure": []
    }
}
```

---

## Project Structure

```
cliq-code/
|-- src_cpp/                       C++ Engine (17 modules)
|   |-- CMakeLists.txt             Build configuration
|   |-- include/cliq-code/              Header files (.hpp)
|   |   |-- file_ops.hpp           File read/write/edit/glob/grep
|   |   |-- command_exec.hpp       Shell command execution
|   |   |-- permission_enforcer.hpp   5-mode permission system
|   |   |-- bash_validation.hpp    Command validation pipeline
|   |   |-- permissions.hpp        Rule-based policy engine
|   |   |-- session.hpp            Conversation persistence
|   |   |-- compact.hpp            Context auto-compaction
|   |   |-- usage.hpp              Token tracking & cost estimation
|   |   |-- json.hpp               Zero-dependency JSON parser
|   |   |-- sse.hpp                Server-Sent Events parser
|   |   |-- hooks.hpp              Pre/post tool-use hooks
|   |   |-- config.hpp             Multi-source config loader
|   |   |-- bootstrap.hpp          Startup phase sequencing
|   |   |-- task_registry.hpp      Sub-agent task management
|   |   |-- team_cron_registry.hpp Team & cron scheduling
|   |   |-- sandbox.hpp            Container detection
|   |   +-- directory_walker.hpp   Fast directory traversal
|   |-- engine/                    Implementation files (.cpp)
|   |-- tests/
|   |   +-- engine_test.cpp        Self-registering test suite
|   +-- bindings/
|       +-- pybind_module.cpp      Python <-> C++ bridge
|
|-- src_python/                    Python Frontend
|   |-- main.py                    CLI entrypoint (REPL + single-shot)
|   |-- brain.py                   LLM routing via LiteLLM
|   |-- safety.py                  Safety Catch approval system
|   |-- ui.py                      Rich terminal interface
|   |-- config.py                  Python-side configuration
|   +-- test_bridge.py             Integration tests
|
|-- rust/                          Original Rust source (reference)
|   +-- crates/runtime/src/        26 original .rs files
|
|-- pyproject.toml                 pip install configuration
+-- README.md                      This file
```

---

## Supported Models

CLIQ Code works with **any model supported by LiteLLM**. Here are the most popular:

| Provider | Model String | Notes |
|----------|-------------|-------|
| **Gemini** | `gemini/gemini-2.0-flash` | Free tier available, fast |
| **Gemini** | `gemini/gemini-2.5-pro-preview-06-05` | Most capable Gemini |
| **OpenAI** | `openai/gpt-4o` | Great all-rounder |
| **OpenAI** | `openai/gpt-4o-mini` | Cheap & fast |
| **Anthropic** | `anthropic/claude-sonnet-4-20250514` | Excellent for code |
| **Anthropic** | `anthropic/claude-3-5-haiku-20241022` | Fast & cheap |
| **Ollama** | `ollama/llama3.1` | Local, free, private |
| **Ollama** | `ollama/codellama` | Local code specialist |

To use Ollama (100% local, no API key needed):
```bash
# Install Ollama: https://ollama.ai
ollama pull llama3.1
python -m src_python.main --model ollama/llama3.1
```

---

## Troubleshooting

### "ModuleNotFoundError: No module named 'litellm'"
```bash
pip install litellm rich InquirerPy
```

### "CMake Error: No CMAKE_CXX_COMPILER found"
You need a C++ compiler. On Windows, install Visual Studio 2022 with the C++ workload. OR just skip the C++ build entirely and use `python -m src_python.main`.

### "GEMINI_API_KEY not set"
```powershell
$env:GEMINI_API_KEY = "your-key-here"
```

### Build fails with "C++20 required"
Update your compiler. MSVC 2022, GCC 10+, or Clang 10+ all support C++20.

### "pybind11 not found" (during CMake)
This is OK! The C++ engine builds without it. To enable the Python bridge:
```bash
pip install pybind11
cmake -B build -S src_cpp  # Re-run CMake
```

---

## Code Stats

| Component | Files | Total Code |
|-----------|-------|-----------|
| C++ Headers | 17 | ~37 KB |
| C++ Implementations | 17 | ~122 KB |
| C++ Tests | 1 | ~6 KB |
| pybind11 Bridge | 1 | ~5 KB |
| Python Frontend | 6 | ~25 KB |
| **Total** | **42** | **~195 KB** |

---

## License

Distributed under the MIT License. See `LICENSE` for more information.
