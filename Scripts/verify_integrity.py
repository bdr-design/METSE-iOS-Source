#!/usr/bin/env python3
from pathlib import Path
import hashlib, sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SOURCE_INTEGRITY_SHA256.txt"
IGNORE_ROOTS = {".git", ".ci-output", "DerivedData", "__pycache__"}
errors = []

expected = {}
for i, line in enumerate(MANIFEST.read_text(encoding="utf-8").splitlines(), 1):
    if not line.strip():
        continue
    try:
        digest, rel = line.split("  ", 1)
    except ValueError:
        errors.append(f"malformed manifest line {i}")
        continue
    expected[rel] = digest.lower()

actual_files = []
for p in ROOT.rglob("*"):
    if not p.is_file():
        continue
    rel = p.relative_to(ROOT).as_posix()
    if rel == "SOURCE_INTEGRITY_SHA256.txt":
        continue
    if any(part in IGNORE_ROOTS for part in p.relative_to(ROOT).parts):
        continue
    actual_files.append((rel, p))

actual = {rel for rel, _ in actual_files}
for rel in sorted(actual - set(expected)):
    errors.append(f"unmanifested file: {rel}")
for rel in sorted(set(expected) - actual):
    errors.append(f"missing file: {rel}")
for rel, wanted in sorted(expected.items()):
    p = ROOT / rel
    if not p.is_file():
        continue
    got = hashlib.sha256(p.read_bytes()).hexdigest()
    if got != wanted:
        errors.append(f"hash mismatch: {rel} expected={wanted} actual={got}")

if errors:
    print("SOURCE INTEGRITY: FAIL")
    for error in errors:
        print(" -", error)
    print("--- BEGIN ACTUAL SOURCE MANIFEST ---")
    for rel, p in sorted(actual_files):
        print(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {rel}")
    print("--- END ACTUAL SOURCE MANIFEST ---")
    sys.exit(1)
print(f"SOURCE INTEGRITY: PASS ({len(expected)} files)")
