#!/usr/bin/env python3
from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[1]
EX={'MANIFEST.sha256','.DS_Store'}; SKIP={'.git','Binaries','Intermediate','Saved','DerivedDataCache','__pycache__'}
rows=[]
for p in sorted(ROOT.rglob('*')):
    if not p.is_file() or p.name in EX or any(x in SKIP for x in p.parts): continue
    h=hashlib.sha256(p.read_bytes()).hexdigest(); rows.append(f'{h}  {p.relative_to(ROOT).as_posix()}')
(ROOT/'MANIFEST.sha256').write_text('\n'.join(rows)+'\n')
print(f'wrote {len(rows)} hashes')
