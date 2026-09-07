#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/Scripts/verify_manifest.py"
python3 "$ROOT/Scripts/guardrails.py"
python3 -m py_compile "$ROOT/Scripts/"*.py
echo "STATIC GATES: PASS"
