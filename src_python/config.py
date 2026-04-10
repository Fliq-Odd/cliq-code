# ─────────────────────────────────────────────────────────────────────
# fliq.config — Configuration & model provider management
# Supports model-agnostic LLM routing via LiteLLM
# ─────────────────────────────────────────────────────────────────────

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


CONFIG_FILENAME = ".fliq.json"
DEFAULT_MODEL = "gemini/gemini-2.5-flash"


@dataclass
class FliqConfig:
    """Runtime configuration for the fliq CLI agent."""

    model: str = DEFAULT_MODEL
    api_key: Optional[str] = None
    temperature: float = 0.2
    max_tokens: int = 4096
    permission_mode: str = "workspace-write"  # read-only | workspace-write | prompt | allow | danger-full-access
    workspace_root: str = field(default_factory=lambda: os.getcwd())
    safety_catch: bool = True  # require approval before running commands
    stream: bool = True
    context_files: list[str] = field(default_factory=list)  # auto-detected project files

    @classmethod
    def load(cls, workspace_dir: Optional[str] = None) -> FliqConfig:
        """Load config from .fliq.json if exists, else defaults."""
        root = Path(workspace_dir or os.getcwd())
        config_path = root / CONFIG_FILENAME

        config = cls(workspace_root=str(root))

        if config_path.exists():
            try:
                with open(config_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                for key, value in data.items():
                    if hasattr(config, key):
                        setattr(config, key, value)
            except (json.JSONDecodeError, OSError):
                pass  # Use defaults on error

        # Override with environment variables
        if env_model := os.environ.get("FLIQ_MODEL"):
            config.model = env_model
        if env_key := os.environ.get("FLIQ_API_KEY"):
            config.api_key = env_key

        # Auto-detect common API keys from standard env vars
        if config.api_key is None:
            for env_var in ("GEMINI_API_KEY", "OPENAI_API_KEY",
                            "ANTHROPIC_API_KEY", "TOGETHER_API_KEY"):
                if key := os.environ.get(env_var):
                    config.api_key = key
                    break

        return config

    def save(self, workspace_dir: Optional[str] = None) -> Path:
        """Persist config to .fliq.json."""
        root = Path(workspace_dir or self.workspace_root)
        config_path = root / CONFIG_FILENAME
        data = {
            "model": self.model,
            "temperature": self.temperature,
            "max_tokens": self.max_tokens,
            "permission_mode": self.permission_mode,
            "safety_catch": self.safety_catch,
            "stream": self.stream,
        }
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        return config_path

    def auto_detect_context(self) -> list[str]:
        """Auto-detect project context files (package.json, requirements.txt, etc.)."""
        root = Path(self.workspace_root)
        context_markers = [
            "package.json", "requirements.txt", "pyproject.toml",
            "Cargo.toml", "go.mod", "pom.xml", "build.gradle",
            "Makefile", "CMakeLists.txt", ".gitignore",
            "Dockerfile", "docker-compose.yml",
        ]
        detected = []
        for marker in context_markers:
            marker_path = root / marker
            if marker_path.exists():
                detected.append(str(marker_path))
        self.context_files = detected
        return detected
