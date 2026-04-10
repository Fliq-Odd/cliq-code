# ─────────────────────────────────────────────────────────────────────
# fliq.safety — The Safety Catch system
# CRITICAL: Never execute a command without explicit user approval
# ─────────────────────────────────────────────────────────────────────

from __future__ import annotations

import re
from dataclasses import dataclass
from enum import Enum
from typing import Optional

# Try to import InquirerPy for interactive prompts
try:
    from InquirerPy import inquirer
    from InquirerPy.base.control import Choice
    INQUIRER_AVAILABLE = True
except ImportError:
    INQUIRER_AVAILABLE = False

# Try to import Rich for styled output
try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.syntax import Syntax
    from rich.text import Text
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False


class SafetyAction(Enum):
    """User decision on a potentially dangerous action."""
    APPROVE = "approve"
    DENY = "deny"
    EDIT = "edit"


@dataclass
class SafetyVerdict:
    """Result of a safety check."""
    action: SafetyAction
    original_command: str
    edited_command: Optional[str] = None
    reason: Optional[str] = None

    @property
    def command(self) -> str:
        """Return the final command to execute (edited or original)."""
        if self.action == SafetyAction.EDIT and self.edited_command:
            return self.edited_command
        return self.original_command


# ── Destructive command detection ─────────────────────────────────────

DESTRUCTIVE_PATTERNS = [
    r"\brm\b",
    r"\brmdir\b",
    r"\bdel\b",
    r"\bformat\b",
    r"\bmkfs\b",
    r"\bdd\b",
    r"\bshred\b",
    r"\bwipe\b",
    r"DROP\s+(TABLE|DATABASE)",
    r"DELETE\s+FROM",
    r"TRUNCATE\s+TABLE",
    r"\bgit\s+push\b.*--force",
    r"\bgit\s+reset\b.*--hard",
    r"\bgit\s+clean\b.*-fd",
    r"\bchmod\b.*777",
    r"\bchown\b",
    r"\bsudo\b",
    r"\bkill\b.*-9",
    r"\bpkill\b",
    r"\bkillall\b",
    r"\bnpm\s+publish\b",
    r"\bpip\s+install\b",
    r"\bcargo\s+publish\b",
    r"\bcurl\b.*\|\s*(sh|bash)",
    r"\bwget\b.*\|\s*(sh|bash)",
]

DESTRUCTIVE_REGEX = re.compile("|".join(DESTRUCTIVE_PATTERNS), re.IGNORECASE)


def is_destructive_command(command: str) -> bool:
    """Check if a command matches known destructive patterns."""
    return bool(DESTRUCTIVE_REGEX.search(command))


def get_danger_level(command: str) -> str:
    """Classify the danger level of a command."""
    command_lower = command.lower().strip()

    # Critical danger
    critical = ["rm -rf /", "rm -rf /*", "dd if=/dev/zero", "mkfs", "format c:"]
    for pattern in critical:
        if pattern in command_lower:
            return "CRITICAL"

    # High danger
    if any(p in command_lower for p in ["rm -rf", "drop database", "git push --force"]):
        return "HIGH"

    # Medium danger (any destructive match)
    if is_destructive_command(command):
        return "MEDIUM"

    return "LOW"


# ── The Safety Catch ──────────────────────────────────────────────────

def safety_catch(command: str, description: Optional[str] = None) -> SafetyVerdict:
    """
    THE SAFETY CATCH — The core safety mechanism.

    Whenever the AI generates a shell command, this function MUST be called.
    It pauses execution and asks the user for explicit approval.
    """
    danger_level = get_danger_level(command)
    is_dangerous = danger_level in ("CRITICAL", "HIGH", "MEDIUM")

    if RICH_AVAILABLE:
        console = Console()
        console.print()

        # Build the display panel
        if danger_level == "CRITICAL":
            style = "bold white on red"
            border_style = "red"
            header = "CRITICAL -- DESTRUCTIVE COMMAND DETECTED"
        elif danger_level == "HIGH":
            style = "bold yellow on red"
            border_style = "red"
            header = "HIGH RISK -- Potentially Destructive"
        elif danger_level == "MEDIUM":
            style = "bold black on yellow"
            border_style = "yellow"
            header = "CAUTION -- Potentially Modifying"
        else:
            style = "bold white on blue"
            border_style = "cyan"
            header = "Command Execution Request"

        # Display the command in a styled panel
        syntax = Syntax(command, "bash", theme="monokai", line_numbers=False)
        panel = Panel(
            syntax,
            title=f"[{style}] {header} [{style}]",
            border_style=border_style,
            padding=(1, 2),
        )
        console.print(panel)

        if description:
            console.print(f"  [dim]{description}[/dim]")
        console.print(f"  Danger Level: [{border_style}]{danger_level}[/{border_style}]")
        console.print()
    else:
        # Fallback plain output
        print(f"\n{'='*60}")
        print(f"  COMMAND: {command}")
        if description:
            print(f"  DESC:    {description}")
        print(f"  DANGER:  {danger_level}")
        print(f"{'='*60}")

    # Interactive prompt
    if INQUIRER_AVAILABLE:
        choices = [
            Choice(value="approve", name="[Y] Yes, execute this command"),
            Choice(value="deny",    name="[N] No, skip this command"),
            Choice(value="edit",    name="[E] Edit the command before running"),
        ]

        action = inquirer.select(
            message="Execute this command?",
            choices=choices,
            default="deny" if is_dangerous else "approve",
        ).execute()

        if action == "edit":
            edited = inquirer.text(
                message="Enter the modified command:",
                default=command,
            ).execute()
            return SafetyVerdict(
                action=SafetyAction.EDIT,
                original_command=command,
                edited_command=edited,
            )

        return SafetyVerdict(
            action=SafetyAction.APPROVE if action == "approve" else SafetyAction.DENY,
            original_command=command,
        )
    else:
        # Fallback: simple y/n prompt
        default = "n" if is_dangerous else "y"
        response = input(f"  Execute? [y/N/e(dit)]: ").strip().lower()
        if not response:
            response = default

        if response in ("y", "yes"):
            return SafetyVerdict(
                action=SafetyAction.APPROVE,
                original_command=command,
            )
        elif response in ("e", "edit"):
            edited = input("  Enter modified command: ").strip()
            return SafetyVerdict(
                action=SafetyAction.EDIT,
                original_command=command,
                edited_command=edited or command,
            )
        else:
            return SafetyVerdict(
                action=SafetyAction.DENY,
                original_command=command,
                reason="User denied command execution",
            )
