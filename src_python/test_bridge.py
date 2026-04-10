"""
fliq C++ ↔ Python bridge test
Run this after building the pybind11 module to verify the bridge works.

Usage:
    python -m src_python.test_bridge
"""

from __future__ import annotations

import os
import sys
import tempfile


def test_pure_python_fallback():
    """Test the Python-only path (no C++ engine required)."""
    print("=" * 60)
    print("  fliq Bridge Test — Pure Python Fallback")
    print("=" * 60)

    # Test 1: Config loading
    print("\n  [1/5] Testing config loader...")
    from src_python.config import FliqConfig
    config = FliqConfig.load()
    assert config.model is not None
    assert config.safety_catch is True
    print(f"        ✅ Config loaded — model={config.model}")

    # Test 2: Safety catch module
    print("\n  [2/5] Testing safety catch...")
    from src_python.safety import is_destructive_command, get_danger_level
    assert is_destructive_command("rm -rf /")
    assert not is_destructive_command("ls -la")
    assert get_danger_level("rm -rf /") == "CRITICAL"
    assert get_danger_level("git log") == "LOW"
    print("        ✅ Safety detection works correctly")

    # Test 3: UI module
    print("\n  [3/5] Testing terminal UI...")
    from src_python.ui import TerminalUI
    ui = TerminalUI()
    print("        ✅ TerminalUI initialized")

    # Test 4: Context auto-detection
    print("\n  [4/5] Testing project context detection...")
    detected = config.auto_detect_context()
    print(f"        ✅ Detected {len(detected)} context file(s)")
    for f in detected[:3]:
        print(f"           📄 {os.path.basename(f)}")

    # Test 5: Command extraction
    print("\n  [5/5] Testing command extraction from AI responses...")
    from src_python.main import extract_bash_commands
    test_response = """
Here's how to list the files:

```bash
ls -la
echo "hello world"
```

And to check git status:

```sh
git status
```
"""
    commands = extract_bash_commands(test_response)
    assert len(commands) == 3
    assert "ls -la" in commands
    assert 'echo "hello world"' in commands
    assert "git status" in commands
    print(f"        ✅ Extracted {len(commands)} commands correctly")

    print("\n" + "=" * 60)
    print("  ALL TESTS PASSED ✅")
    print("=" * 60 + "\n")


def test_cpp_bridge():
    """Test the C++ engine bridge (requires compiled pybind11 module)."""
    try:
        import fliq_core
    except ImportError:
        print("\n  ⚠️  C++ engine module (fliq_core) not found.")
        print("     This is expected until you build with CMake + pybind11.")
        print("     The pure-Python path works independently.\n")
        return False

    print("\n" + "=" * 60)
    print("  fliq Bridge Test — C++ Engine")
    print("=" * 60)

    # Test file operations
    print("\n  [1/3] Testing C++ file operations...")
    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt",
                                      delete=False) as f:
        f.write("line one\nline two\nline three")
        tmp_path = f.name

    result = fliq_core.read_file(tmp_path)
    assert result.file.total_lines == 3
    print(f"        ✅ read_file: {result.file.total_lines} lines")

    os.unlink(tmp_path)

    # Test permission enforcer
    print("\n  [2/3] Testing C++ permission enforcer...")
    assert fliq_core.is_read_only_command("cat file.txt") is True
    assert fliq_core.is_read_only_command("rm -rf /") is False
    print("        ✅ Permission heuristic works")

    # Test directory walker
    print("\n  [3/3] Testing C++ directory walker...")
    stats = fliq_core.get_directory_stats(".")
    print(f"        ✅ Walker: {stats.total_files} files, "
          f"{stats.total_size_bytes:,} bytes")

    print("\n" + "=" * 60)
    print("  C++ ENGINE TESTS PASSED ✅")
    print("=" * 60 + "\n")
    return True


if __name__ == "__main__":
    test_pure_python_fallback()
    test_cpp_bridge()
