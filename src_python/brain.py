# ─────────────────────────────────────────────────────────────────────
# fliq.brain — Model-agnostic LLM routing via LiteLLM
# Supports OpenAI, Anthropic, Gemini, Llama, DeepSeek out of the box
# ─────────────────────────────────────────────────────────────────────

from __future__ import annotations

import json
import os
import sys
import time
from dataclasses import dataclass, field
from typing import Any, Generator, Optional

from .config import FliqConfig

# Try to import litellm; provide clear error if missing
try:
    import litellm
    from litellm import completion, acompletion
    LITELLM_AVAILABLE = True
except ImportError:
    LITELLM_AVAILABLE = False


# ── System prompt ─────────────────────────────────────────────────────

SYSTEM_PROMPT = """You are fliq, a lightning-fast AI coding assistant running in the user's terminal.

You have access to the following tools:
- read_file: Read a file's contents
- write_file: Write content to a file
- edit_file: Edit a file by replacing text
- bash: Execute a shell command
- grep_search: Search files with regex
- glob_search: Find files matching patterns
- list_directory: List directory contents

IMPORTANT RULES:
1. Always explain what you're about to do before doing it.
2. When you need to run a command, output it in a ```bash block.
3. When you need to edit a file, show the changes clearly.
4. Be concise but thorough in your explanations.
5. If you're unsure about something, ask the user.
6. Never run destructive commands without explicit user approval.
7. Always respect the workspace boundaries.

You are working in the user's project directory. Be helpful, precise, and safe."""


@dataclass
class Message:
    """A single conversation message."""
    role: str       # "system" | "user" | "assistant" | "tool"
    content: str
    tool_calls: Optional[list[dict]] = None
    name: Optional[str] = None


@dataclass
class ToolCall:
    """A parsed tool call from the LLM."""
    tool_name: str
    arguments: dict[str, Any]
    raw: str = ""


@dataclass
class BrainResponse:
    """Response from the LLM brain."""
    content: str = ""
    tool_calls: list[ToolCall] = field(default_factory=list)
    usage: dict[str, int] = field(default_factory=dict)
    model: str = ""
    finish_reason: str = ""
    elapsed_ms: float = 0.0


@dataclass
class ConversationHistory:
    """Manages the conversation message history."""
    messages: list[Message] = field(default_factory=list)
    max_messages: int = 50  # Context window management

    def add_system(self, content: str) -> None:
        # Only add system prompt once at the start
        if not self.messages or self.messages[0].role != "system":
            self.messages.insert(0, Message(role="system", content=content))

    def add_user(self, content: str) -> None:
        self.messages.append(Message(role="user", content=content))

    def add_assistant(self, content: str) -> None:
        self.messages.append(Message(role="assistant", content=content))

    def add_tool_result(self, name: str, content: str) -> None:
        self.messages.append(Message(role="tool", content=content, name=name))

    def to_litellm_messages(self) -> list[dict[str, str]]:
        """Convert to litellm-compatible message format."""
        result = []
        for msg in self.messages:
            entry: dict[str, Any] = {"role": msg.role, "content": msg.content}
            if msg.name:
                entry["name"] = msg.name
            result.append(entry)
        return result

    def compact(self) -> None:
        """Trim old messages to stay within context limits."""
        if len(self.messages) <= self.max_messages:
            return
        # Keep system prompt + last N messages
        system = [m for m in self.messages if m.role == "system"]
        recent = self.messages[-(self.max_messages - len(system)):]
        self.messages = system + recent


class Brain:
    """Model-agnostic LLM interface powered by LiteLLM."""

    def __init__(self, config: FliqConfig):
        self.config = config
        self.history = ConversationHistory()
        self.history.add_system(SYSTEM_PROMPT)
        self._total_input_tokens = 0
        self._total_output_tokens = 0

        if not LITELLM_AVAILABLE:
            raise ImportError(
                "litellm is required for the Brain module.\n"
                "Install it with: pip install litellm"
            )

        # Configure litellm
        litellm.drop_params = True  # Don't error on unsupported params

        # Set API key if provided
        if config.api_key:
            # LiteLLM auto-routes based on model prefix
            if "gemini" in config.model.lower():
                os.environ.setdefault("GEMINI_API_KEY", config.api_key)
            elif "gpt" in config.model.lower() or "o1" in config.model.lower():
                os.environ.setdefault("OPENAI_API_KEY", config.api_key)
            elif "claude" in config.model.lower():
                os.environ.setdefault("ANTHROPIC_API_KEY", config.api_key)

    def send(self, user_message: str) -> BrainResponse:
        """Send a message and get a response (non-streaming)."""
        self.history.add_user(user_message)
        self.history.compact()

        start = time.monotonic()

        try:
            response = completion(
                model=self.config.model,
                messages=self.history.to_litellm_messages(),
                temperature=self.config.temperature,
                max_tokens=self.config.max_tokens,
            )
        except Exception as e:
            return BrainResponse(
                content=f"Error communicating with LLM: {e}",
                finish_reason="error",
                elapsed_ms=(time.monotonic() - start) * 1000,
            )

        elapsed = (time.monotonic() - start) * 1000

        # Parse response
        choice = response.choices[0]
        content = choice.message.content or ""
        finish_reason = choice.finish_reason or ""

        # Track usage
        usage = {}
        if hasattr(response, "usage") and response.usage:
            usage = {
                "input_tokens": getattr(response.usage, "prompt_tokens", 0),
                "output_tokens": getattr(response.usage, "completion_tokens", 0),
            }
            self._total_input_tokens += usage.get("input_tokens", 0)
            self._total_output_tokens += usage.get("output_tokens", 0)

        self.history.add_assistant(content)

        return BrainResponse(
            content=content,
            usage=usage,
            model=self.config.model,
            finish_reason=finish_reason,
            elapsed_ms=elapsed,
        )

    def stream(self, user_message: str) -> Generator[str, None, BrainResponse]:
        """Send a message and stream the response token by token."""
        self.history.add_user(user_message)
        self.history.compact()

        start = time.monotonic()
        full_content = ""

        try:
            response = completion(
                model=self.config.model,
                messages=self.history.to_litellm_messages(),
                temperature=self.config.temperature,
                max_tokens=self.config.max_tokens,
                stream=True,
            )

            for chunk in response:
                if hasattr(chunk, "choices") and chunk.choices:
                    delta = chunk.choices[0].delta
                    if hasattr(delta, "content") and delta.content:
                        full_content += delta.content
                        yield delta.content

        except Exception as e:
            error_msg = f"\n\nError during streaming: {e}"
            full_content += error_msg
            yield error_msg

        elapsed = (time.monotonic() - start) * 1000
        self.history.add_assistant(full_content)

    @property
    def total_tokens(self) -> dict[str, int]:
        return {
            "input": self._total_input_tokens,
            "output": self._total_output_tokens,
            "total": self._total_input_tokens + self._total_output_tokens,
        }

    def reset(self) -> None:
        """Clear conversation history (keeps system prompt)."""
        self.history = ConversationHistory()
        self.history.add_system(SYSTEM_PROMPT)
