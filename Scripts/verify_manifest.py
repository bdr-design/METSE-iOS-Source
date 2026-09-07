#!/usr/bin/env python3
from pathlib import Path
import hashlib,sys
ROOT=Path(__file__).resolve().parents[1]; manifest=ROOT/'MANIFEST.sha256'; errors=[]
if not manifest.exists(): print('missing MANIFEST.sha256');sys.exit(1)
seen=set()
for line in manifest.read_text().splitlines():
    if not line.strip(): continue
    h,rel=line.split('  ',1); p=ROOT/rel; seen.add(rel)
    if not p.exists(): errors.append(f'missing: {rel}'); continue
    got=hashlib.sha256(p.read_bytes()).hexdigest()
    if got!=h: errors.append(f'hash mismatch: {rel}')
if errors:
    print('MANIFEST: FAIL'); [print(' -',x) for x in errors];sys.exit(1)
print(f'MANIFEST: PASS ({len(seen)} files)')
