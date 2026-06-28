import sys
import os

# Ensure Unicode characters print correctly on Windows terminals
if sys.platform.startswith("win"):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")

# Ensure the root directory is in the path so relative imports inside src_python work
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

from src_python.main import main

if __name__ == "__main__":
    main()
