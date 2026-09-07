#!/usr/bin/env python3
from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'MANIFEST.sha256'
errors = []

PROTECTED_ROOTS = [ROOT / 'Source', ROOT / 'Config']
PROTECTED_SINGLE_FILES = [ROOT / 'METSE.uproject']

if not MANIFEST.exists():
    print('MANIFEST: FAIL')
    print(' - missing MANIFEST.sha256')
    sys.exit(1)

seen = set()
for raw_line in MANIFEST.read_text(encoding='utf-8').splitlines():
    if not raw_line.strip():
        continue
    try:
        expected_hash, rel = raw_line.split('  ', 1)
    except ValueError:
        errors.append(f'malformed manifest line: {raw_line!r}')
        continue

    path = ROOT / rel
    seen.add(rel)
    if not path.exists():
        errors.append(f'missing: {rel}')
        continue
    if not path.is_file():
        errors.append(f'not a file: {rel}')
        continue

    actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual_hash != expected_hash:
        errors.append(
            f'hash mismatch: {rel} | expected={expected_hash} | actual={actual_hash}'
        )

protected_files = []
for root in PROTECTED_ROOTS:
    if root.exists():
        protected_files.extend(path for path in root.rglob('*') if path.is_file())
for path in PROTECTED_SINGLE_FILES:
    if path.exists() and path.is_file():
        protected_files.append(path)

for path in sorted(set(protected_files)):
    rel = path.relative_to(ROOT).as_posix()
    if rel not in seen:
        actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        errors.append(f'unmanifested protected file: {rel} | actual={actual_hash}')

if errors:
    print('MANIFEST: FAIL')
    for error in errors:
        print(' -', error)
    sys.exit(1)

print(f'MANIFEST: PASS ({len(seen)} files; protected scope fully covered)')
