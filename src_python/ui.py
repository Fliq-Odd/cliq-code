# -----------------------------------------------------------------------
# fliq.ui -- Terminal interface using Rich
# -----------------------------------------------------------------------

from __future__ import annotations

import os
import sys
from typing import Optional

try:
    from rich.console import Console
    from rich.live import Live
    from rich.markdown import Markdown
    from rich.panel import Panel
    from rich.progress import Progress, SpinnerColumn, TextColumn
    from rich.syntax import Syntax
    from rich.table import Table
    from rich.text import Text
    from rich.theme import Theme
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False


# -- Custom theme ----------------------------------------------------------

FLIQ_THEME = Theme({
    "fliq.header":    "bold cyan",
    "fliq.prompt":    "bold green",
    "fliq.info":      "dim cyan",
    "fliq.warning":   "bold yellow",
    "fliq.error":     "bold red",
    "fliq.success":   "bold green",
    "fliq.dim":       "dim white",
    "fliq.highlight": "bold magenta",
    "fliq.model":     "bold blue",
    "fliq.tokens":    "dim yellow",
    "fliq.logo":      "bold cyan",
    "fliq.bolt":      "bold yellow",
    "fliq.label":     "dim white",
    "fliq.value":     "bold white",
})

# -- ASCII logo ------------------------------------------------------------


class TerminalUI:
    """Rich-powered terminal interface for CLIQ Code."""

    def __init__(self):
        if RICH_AVAILABLE:
            self.console = Console(theme=FLIQ_THEME)
        else:
            self.console = None  # type: ignore

    # -- Banner ------------------------------------------------------------

    def _print_logo(self) -> None:
        """Print the ASCII logo directly via print() to avoid Rich escaping."""
        # Using print() so backslashes render exactly as-is
        print("   ____ _ _          ____          _      ")
        print("  / ___| (_) __ _   / ___|___   __| | ___ ")
        print(" | |   | | |/ _` | | |   / _ \\ / _` |/ _ \\")
        print(" | |___| | | (_| | | |__| (_) | (_| |  __/")
        print("  \\____|_|_|\\__, |  \\____\\___/ \\__,_|\\___|")
        print("               |_|                        ")
        print()

    def print_banner(self, model: str, version: str = "0.1.0") -> None:
        """Print the startup banner."""
        if self.console:
            self._print_logo()
            self.console.print(f"[dim]{'v' + version:>45}[/dim]")
            self.console.print()

            # Status lines - clean, no emojis
            self.console.print(
                f"  [fliq.label]  model [/fliq.label]"
                f"[fliq.model]{model}[/fliq.model]"
            )
            self.console.print(
                f"  [fliq.label]    dir [/fliq.label]"
                f"[fliq.info]{os.getcwd()}[/fliq.info]"
            )
            self.console.print(
                f"  [fliq.label] safety [/fliq.label]"
                f"[fliq.success]ON[/fliq.success]"
                f"[fliq.dim] -- commands require approval[/fliq.dim]"
            )
            self.console.print()
            self.console.print(
                "  [fliq.dim]Type [/fliq.dim]"
                "[fliq.highlight]/help[/fliq.highlight]"
                "[fliq.dim] for commands, [/fliq.dim]"
                "[fliq.highlight]/quit[/fliq.highlight]"
                "[fliq.dim] to exit[/fliq.dim]"
            )
            self.console.print()
        else:
            self._print_logo()
            print(f"  v{version}")
            print(f"  model  {model}")
            print(f"  dir    {os.getcwd()}")
            print(f"  safety ON -- commands require approval")
            print(f"  Type /help for commands, /quit to exit\n")

    # -- User prompt -------------------------------------------------------

    def get_user_input(self) -> str:
        """Get input from the user with a styled prompt."""
        if self.console:
            self.console.print("[fliq.prompt]>[/fliq.prompt] ", end="")
            try:
                return input()
            except (EOFError, KeyboardInterrupt):
                return "/quit"
        else:
            try:
                return input("> ")
            except (EOFError, KeyboardInterrupt):
                return "/quit"

    # -- Response rendering ------------------------------------------------

    def print_response(self, content: str) -> None:
        """Render a response with markdown formatting."""
        if self.console:
            self.console.print()
            md = Markdown(content)
            self.console.print(md)
            self.console.print()
        else:
            print(f"\n{content}\n")

    def print_streaming_start(self) -> None:
        if self.console:
            self.console.print()

    def print_streaming_token(self, token: str) -> None:
        sys.stdout.write(token)
        sys.stdout.flush()

    def print_streaming_end(self) -> None:
        print()

    # -- Code display ------------------------------------------------------

    def print_code(self, code: str, language: str = "python",
                   title: Optional[str] = None) -> None:
        if self.console:
            syntax = Syntax(code, language, theme="monokai",
                            line_numbers=True, word_wrap=True)
            if title:
                panel = Panel(syntax, title=title, border_style="green")
                self.console.print(panel)
            else:
                self.console.print(syntax)
        else:
            print(f"```{language}")
            print(code)
            print("```")

    # -- Status messages ---------------------------------------------------

    def print_info(self, message: str) -> None:
        if self.console:
            self.console.print(f"  [fliq.info][i] {message}[/fliq.info]")
        else:
            print(f"  [i] {message}")

    def print_success(self, message: str) -> None:
        if self.console:
            self.console.print(f"  [fliq.success][+] {message}[/fliq.success]")
        else:
            print(f"  [+] {message}")

    def print_warning(self, message: str) -> None:
        if self.console:
            self.console.print(f"  [fliq.warning][!] {message}[/fliq.warning]")
        else:
            print(f"  [!] {message}")

    def print_error(self, message: str) -> None:
        if self.console:
            self.console.print(f"  [fliq.error][x] {message}[/fliq.error]")
        else:
            print(f"  [x] {message}")

    # -- Token usage -------------------------------------------------------

    def print_usage(self, elapsed_ms: float, usage: dict, model: str) -> None:
        if self.console:
            in_tok = usage.get("input_tokens", usage.get("input", 0))
            out_tok = usage.get("output_tokens", usage.get("output", 0))
            self.console.print(
                f"  [fliq.tokens]{elapsed_ms:.0f}ms | "
                f"{in_tok} in / {out_tok} out | "
                f"{model}[/fliq.tokens]"
            )
        else:
            print(f"  {elapsed_ms:.0f}ms | {model}")

    # -- Spinner -----------------------------------------------------------

    def spinner(self, message: str = "Thinking..."):
        if self.console:
            return Progress(
                SpinnerColumn(style="cyan"),
                TextColumn("[fliq.info]{task.description}"),
                console=self.console,
                transient=True,
            )
        return _DummySpinner()

    # -- Help --------------------------------------------------------------

    def print_help(self) -> None:
        if self.console:
            table = Table(
                title="[fliq.header]CLIQ Code commands[/fliq.header]",
                border_style="cyan",
                show_lines=True,
            )
            table.add_column("Command", style="bold cyan", min_width=15)
            table.add_column("Description", style="dim white")

            commands = [
                ("/help",    "Show this help message"),
                ("/quit",    "Exit CLIQ Code"),
                ("/clear",   "Clear conversation history"),
                ("/model",   "Change the active LLM model"),
                ("/config",  "Show current configuration"),
                ("/safety",  "Toggle safety catch on/off"),
                ("/context", "Show detected project context"),
                ("/tokens",  "Show total token usage"),
            ]
            for cmd, desc in commands:
                table.add_row(cmd, desc)

            self.console.print()
            self.console.print(table)
            self.console.print()
        else:
            print("\nAvailable commands:")
            print("  /help    - Show help")
            print("  /quit    - Exit")
            print("  /clear   - Clear history")
            print("  /model   - Change model")
            print("  /config  - Show config")
            print("  /safety  - Toggle safety catch")
            print("  /context - Show project context")
            print("  /tokens  - Show token usage\n")


class _DummySpinner:
    """Fallback for when Rich is not available."""
    def __enter__(self):
        print("  Thinking...")
        return self
    def __exit__(self, *args):
        pass
    def add_task(self, *args, **kwargs):
        pass
