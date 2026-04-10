#!/usr/bin/env python3
# ─────────────────────────────────────────────────────────────────────
# fliq — Main CLI entrypoint
# Lightning-fast AI coding agent with C++ engine & Python UI
# ─────────────────────────────────────────────────────────────────────

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from typing import Optional

from .brain import Brain, BrainResponse
from .config import FliqConfig
from .safety import SafetyAction, safety_catch
from .ui import TerminalUI


def extract_bash_commands(content: str) -> list[str]:
    """Extract bash commands from markdown code blocks in the AI response."""
    pattern = r"```(?:bash|sh|shell|cmd|powershell)\n(.*?)```"
    matches = re.findall(pattern, content, re.DOTALL)
    commands = []
    for match in matches:
        for line in match.strip().split("\n"):
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                commands.append(stripped)
    return commands


def run_command_with_safety(command: str, ui: TerminalUI,
                            config: FliqConfig,
                            description: Optional[str] = None) -> Optional[str]:
    """Execute a command through the Safety Catch system."""
    if config.safety_catch:
        verdict = safety_catch(command, description)

        if verdict.action == SafetyAction.DENY:
            ui.print_warning("Command skipped by user.")
            return None

        final_command = verdict.command
    else:
        final_command = command

    # Execute the command
    try:
        ui.print_info(f"Executing: {final_command}")
        result = subprocess.run(
            final_command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=30,
            cwd=config.workspace_root,
        )
        output = result.stdout
        if result.stderr:
            output += "\n" + result.stderr
        if result.returncode != 0:
            ui.print_warning(f"Exit code: {result.returncode}")
        else:
            ui.print_success("Command completed successfully.")
        return output.strip()
    except subprocess.TimeoutExpired:
        ui.print_error("Command timed out (30s limit).")
        return "Command timed out"
    except Exception as e:
        ui.print_error(f"Command failed: {e}")
        return f"Error: {e}"


def handle_slash_command(command: str, ui: TerminalUI, brain: Brain,
                         config: FliqConfig) -> bool:
    """Handle slash commands. Returns True if the loop should continue."""
    cmd = command.lower().strip()

    if cmd in ("/quit", "/exit", "/q"):
        ui.print_info("Goodbye!")
        return False

    if cmd == "/help":
        ui.print_help()
        return True

    if cmd == "/clear":
        brain.reset()
        ui.print_success("Conversation history cleared.")
        return True

    if cmd == "/config":
        ui.print_info(f"Model: {config.model}")
        ui.print_info(f"Temperature: {config.temperature}")
        ui.print_info(f"Max Tokens: {config.max_tokens}")
        ui.print_info(f"Permission Mode: {config.permission_mode}")
        ui.print_info(f"Safety Catch: {'ON' if config.safety_catch else 'OFF'}")
        ui.print_info(f"Workspace: {config.workspace_root}")
        return True

    if cmd == "/safety":
        config.safety_catch = not config.safety_catch
        state = "ON" if config.safety_catch else "OFF"
        if config.safety_catch:
            ui.print_success(f"Safety Catch: {state} — Commands require approval")
        else:
            ui.print_warning(f"Safety Catch: {state} — Commands will auto-execute!")
        return True

    if cmd == "/context":
        detected = config.auto_detect_context()
        if detected:
            ui.print_info(f"Detected {len(detected)} project files:")
            for f in detected:
                ui.print_info(f"  {os.path.basename(f)}")
        else:
            ui.print_warning("No project context files detected.")
        return True

    if cmd == "/tokens":
        tokens = brain.total_tokens
        ui.print_info(f"Total input tokens:  {tokens['input']:,}")
        ui.print_info(f"Total output tokens: {tokens['output']:,}")
        ui.print_info(f"Total tokens:        {tokens['total']:,}")
        return True

    if cmd.startswith("/model"):
        parts = command.split(maxsplit=1)
        if len(parts) > 1:
            new_model = parts[1].strip()
            config.model = new_model
            brain.config.model = new_model
            ui.print_success(f"Model changed to: {new_model}")
        else:
            ui.print_info(f"Current model: {config.model}")
            ui.print_info("Usage: /model <model_name>")
            ui.print_info("Examples:")
            ui.print_info("  /model gemini/gemini-2.5-flash")
            ui.print_info("  /model gpt-4o")
            ui.print_info("  /model claude-sonnet-4-20250514")
            ui.print_info("  /model together_ai/meta-llama/Meta-Llama-3-8B")
        return True

    ui.print_warning(f"Unknown command: {command}")
    return True


def run_repl(config: FliqConfig) -> None:
    """Run the interactive REPL (Read-Eval-Print Loop)."""
    ui = TerminalUI()
    brain = Brain(config)

    # Auto-detect project context
    context_files = config.auto_detect_context()
    if context_files:
        context_summary = ", ".join(os.path.basename(f) for f in context_files[:5])
        brain.history.messages[0].content += (
            f"\n\nProject context detected: {context_summary}"
        )

    # Print the beautiful startup banner
    ui.print_banner(config.model)

    # Main REPL loop
    while True:
        try:
            user_input = ui.get_user_input()

            if not user_input.strip():
                continue

            # Handle slash commands
            if user_input.startswith("/"):
                if not handle_slash_command(user_input, ui, brain, config):
                    break
                continue

            # Send to brain
            if config.stream:
                ui.print_streaming_start()
                full_response = ""
                for token in brain.stream(user_input):
                    ui.print_streaming_token(token)
                    full_response += token
                ui.print_streaming_end()
            else:
                with ui.spinner("Thinking...") as spinner:
                    spinner.add_task("thinking", description="🧠 Thinking...")
                    response = brain.send(user_input)
                    full_response = response.content

                ui.print_response(full_response)
                ui.print_usage(response.elapsed_ms, response.usage, response.model)

            # Extract and offer to execute any bash commands
            commands = extract_bash_commands(full_response)
            if commands:
                ui.print_info(f"Found {len(commands)} command(s) to execute:")
                for cmd in commands:
                    output = run_command_with_safety(cmd, ui, config)
                    if output:
                        # Feed the output back to the brain
                        brain.history.add_tool_result("bash", output)

        except KeyboardInterrupt:
            ui.print_info("\nInterrupted. Type /quit to exit.")
            continue
        except Exception as e:
            ui.print_error(f"Unexpected error: {e}")
            continue


def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="fliq",
        description="⚡ fliq — Lightning-fast AI coding agent",
    )
    parser.add_argument(
        "--model", "-m",
        default=None,
        help="LLM model to use (e.g., gemini/gemini-2.5-flash, gpt-4o, claude-sonnet-4-20250514)",
    )
    parser.add_argument(
        "--api-key", "-k",
        default=None,
        help="API key for the LLM provider",
    )
    parser.add_argument(
        "--no-safety",
        action="store_true",
        help="Disable the Safety Catch (commands auto-execute — use with caution!)",
    )
    parser.add_argument(
        "--no-stream",
        action="store_true",
        help="Disable streaming responses",
    )
    parser.add_argument(
        "--workspace", "-w",
        default=None,
        help="Set the workspace root directory",
    )
    parser.add_argument(
        "prompt",
        nargs="*",
        help="Optional initial prompt (runs in single-shot mode)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Main entrypoint."""
    parser = build_parser()
    args = parser.parse_args(argv)

    # Load config
    config = FliqConfig.load(args.workspace)

    # Apply CLI overrides
    if args.model:
        config.model = args.model
    if args.api_key:
        config.api_key = args.api_key
    if args.no_safety:
        config.safety_catch = False
    if args.no_stream:
        config.stream = False
    if args.workspace:
        config.workspace_root = os.path.abspath(args.workspace)

    # Single-shot mode
    if args.prompt:
        prompt = " ".join(args.prompt)
        ui = TerminalUI()
        brain = Brain(config)
        config.auto_detect_context()

        response = brain.send(prompt)
        ui.print_response(response.content)

        commands = extract_bash_commands(response.content)
        for cmd in commands:
            run_command_with_safety(cmd, ui, config)

        return 0

    # Interactive REPL mode
    try:
        run_repl(config)
    except Exception as e:
        print(f"\n❌ Fatal error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
