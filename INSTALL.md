# CLIQ Code Installation & Usage Guide (by fliq odd)

> This guide is written specifically for **Windows** users. If you're on Mac/Linux, check README.md.

---

## What is CLIQ Code?

CLIQ Code (developed by **fliq odd**) is like your own **Claude Code / Cursor / Aider** but open source and works with ANY AI model.

You type in English → CLIQ Code uses AI to → Read/Write files, Run commands, Fix bugs, Build projects.

**It runs in your TERMINAL (PowerShell/CMD).**

---

## HOW TO INSTALL & RUN (Pick One Method)

---

### METHOD 1: Standalone Executable (Absolute Fastest — No Python Required)

You do NOT need Python, CMake, git, or compiler setups.
1. Download the latest `cliq-code.exe` from the [Releases](https://github.com/fliq-odd/cliq-code/releases) section.
2. Put the `cliq-code.exe` file in a directory of your choice (e.g. `C:\Program Files\cliq-code`).
3. Add that directory to your Windows system Environment Variable `PATH` so you can launch it from any terminal.

---

### METHOD 2: Python package (Requires Python 3.10+)

If you want to run CLIQ Code as a standard Python tool:

1. Open PowerShell and install directly from GitHub:
   ```powershell
   pip install git+https://github.com/fliq-odd/cliq-code.git
   ```

**What this installs:**
| Package | Purpose |
|---------|---------|
| `litellm` | Talks to any AI (Gemini, GPT, Claude, Llama) |
| `rich` | Makes the terminal look beautiful (colors, markdown) |
| `InquirerPy` | Shows interactive yes/no prompts |

---

## Set your AI API key

You need a key from AT LEAST ONE provider. **Gemini is FREE** -- easiest option.

### Option A: Google Gemini (FREE & recommended)

1. Go to: https://aistudio.google.com/apikey
2. Click "Create API Key"
3. Copy the key
4. In PowerShell, run:
   ```powershell
   $env:GEMINI_API_KEY = "paste-your-key-here"
   ```

### Option B: OpenAI (GPT-4)

1. Go to: https://platform.openai.com/api-keys
2. Create a key
3. Run:
   ```powershell
   $env:OPENAI_API_KEY = "sk-paste-your-key-here"
   ```

### Option C: Anthropic (Claude)

1. Go to: https://console.anthropic.com/settings/keys
2. Create a key
3. Run:
   ```powershell
   $env:ANTHROPIC_API_KEY = "sk-ant-paste-your-key-here"
   ```

> **IMPORTANT:** The `$env:` method only lasts for your current terminal session.
> To make it permanent (so you don't have to type it every time):
> ```powershell
> [System.Environment]::SetEnvironmentVariable("GEMINI_API_KEY", "your-key-here", "User")
> ```
> Then restart your terminal.

---

## Run CLIQ Code

Simply type in your terminal:
```powershell
cliq-code
```

That's it! You should see:

```
 fliq v0.1.0 -- Lightning-fast AI coding agent
 Model: gemini/gemini-2.0-flash
 Workspace: C:\Users\91931\Desktop\fliq odd
 Safety Catch: ACTIVE
 Type /help for commands, /quit to exit

> _
```

Now type anything! Like:
```
> What files are in this project?
> Explain what src_python/brain.py does
> Write a hello world program in Python
```

---

## How to Use CLIQ Code (Full Usage Guide)

### Starting CLIQ Code

| Command | What It Does |
|---------|-------------|
| `cliq-code` | Start interactive mode (REPL) |
| `cliq-code "your question"` | One-shot: ask a question and exit |
| `cliq-code --model openai/gpt-4o` | Use a specific AI model |
| `cliq-code --no-safety` | Skip command approval prompts |

### Inside CLIQ Code -- Slash Commands

Once CLIQ Code is running, you can use these commands:

| Type This | What Happens |
|-----------|-------------|
| `/help` | Shows all commands |
| `/model gemini/gemini-2.0-flash` | Switch AI model |
| `/model openai/gpt-4o` | Switch to GPT-4o |
| `/model anthropic/claude-sonnet-4-20250514` | Switch to Claude |
| `/safety` | Toggle command safety on/off |
| `/config` | Show current settings |
| `/tokens` | Show how many tokens used (cost tracker) |
| `/context` | Show detected project files |
| `/clear` | Wipe conversation history |
| `/quit` | Exit FLIQ |

### Example Conversations

**Ask about code:**
```
> Explain what the file src_cpp/engine/bash_validation.cpp does
```

**Write code:**
```
> Create a Python script that downloads all images from a URL
```

**Fix bugs:**
```
> I'm getting this error: TypeError: 'NoneType' has no len(). The code is in app.py line 42
```

**Run commands:**
```
> List all Python files in this project
```
FLIQ will suggest `find . -name "*.py"` and ask you to approve before running.

**Edit files:**
```
> Add error handling to the database connection in src/db.py
```

---

## Safety System Explained

When FLIQ wants to run a command, it does NOT just run it. Here's what happens:

```
YOU: "Delete all .tmp files"

FLIQ: I'll run this command:

  FLIQ wants to run:
  +-----------------------------+
  |  $ rm *.tmp                 |
  |                             |
  |  Risk: MEDIUM               |
  |  Type: File deletion        |
  +-----------------------------+

  [Y] Allow  [N] Deny  [A] Allow always for this session
```

You press **Y** to allow, **N** to deny. You are ALWAYS in control.

### Risk Levels

| Level | Examples |
|-------|---------|
| LOW | `ls`, `cat`, `git status`, `python --version` |
| MEDIUM | `npm install`, `pip install`, `mkdir` |
| HIGH | `rm`, `chmod`, `docker`, `sudo` |
| CRITICAL | `rm -rf /`, `mkfs`, `dd`, fork bombs |

---

## All Supported AI Models

| Model String | Provider | Cost | Speed |
|-------------|----------|------|-------|
| `gemini/gemini-2.0-flash` | Google | FREE | Fast |
| `gemini/gemini-2.5-pro-preview-06-05` | Google | Paid | Medium |
| `openai/gpt-4o` | OpenAI | ~$5/M tokens | Medium |
| `openai/gpt-4o-mini` | OpenAI | ~$0.15/M tokens | Fast |
| `anthropic/claude-sonnet-4-20250514` | Anthropic | ~$3/M tokens | Medium |
| `anthropic/claude-3-5-haiku-20241022` | Anthropic | ~$1/M tokens | Fast |
| `ollama/llama3.1` | Local (Ollama) | FREE | Depends on GPU |
| `ollama/codellama` | Local (Ollama) | FREE | Depends on GPU |

**To use Ollama (100% local, no internet, no API key):**
1. Install from https://ollama.ai
2. `ollama pull llama3.1`
3. `cliq-code --model ollama/llama3.1`

---

## FAQ / Troubleshooting

### "ModuleNotFoundError: No module named 'rich'"
```powershell
pip install rich InquirerPy litellm
```

### "No API key found"
Set it in PowerShell:
```powershell
$env:GEMINI_API_KEY = "your-key"
```

### "Error communicating with LLM"
- Check your internet connection
- Check your API key is correct
- Check the model name is valid (e.g., `gemini/gemini-2.0-flash` not just `gemini`)

### "cmake is not recognized"
**You DON'T need cmake for the Python version!** The C++ engine is an optional performance boost. Just run `cliq-code` directly.

### "How do I make the API key permanent?"
```powershell
[System.Environment]::SetEnvironmentVariable("GEMINI_API_KEY", "your-key", "User")
```
Then restart your terminal/PowerShell.

---

## Optional: Build the C++ Engine (Advanced)

This is ONLY needed if you want maximum performance. The Python version works perfectly on its own.

### Install CMake (if you want to):
```powershell
winget install -e --id Kitware.CMake
```
Then restart your terminal.

### Install a C++ Compiler:
Install [Visual Studio 2022](https://visualstudio.microsoft.com/) -- select "Desktop development with C++" workload during install.

### Build:
```powershell
cmake -B build -S src_cpp
cmake --build build --config Release
```

---

## What Each File Does

```
fliq odd/
|-- src_python/             <-- THIS IS WHAT YOU RUN
|   |-- main.py             <-- Entry point (start here!)
|   |-- brain.py            <-- Talks to AI via LiteLLM
|   |-- safety.py           <-- Asks before dangerous commands
|   |-- ui.py               <-- Makes terminal look nice
|   |-- config.py           <-- Settings & configuration
|   +-- __init__.py         <-- Package marker
|
|-- src_cpp/                <-- C++ ENGINE (optional, for speed)
|   |-- include/fliq/       <-- 17 header files
|   |-- engine/             <-- 17 implementation files
|   |-- tests/              <-- Test suite
|   +-- CMakeLists.txt      <-- Build file (needs CMake)
|
|-- rust/                   <-- ORIGINAL reference code
|   +-- crates/runtime/src/ <-- 26 Rust files we translated
|
|-- README.md               <-- Project overview
|-- INSTALL.md              <-- THIS FILE (you are here!)
+-- pyproject.toml          <-- pip install config
```
