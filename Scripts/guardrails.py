#!/usr/bin/env python3
from pathlib import Path
import json,re,sys
ROOT=Path(__file__).resolve().parents[1]; errors=[]
SKIP_PARTS={'.git','Binaries','Intermediate','Saved','DerivedDataCache','__pycache__','Artifacts','TestResults','BuildLogs'}
TEXT_SUFFIXES={'.py','.sh','.md','.ini','.json','.yml','.yaml','.h','.hpp','.cpp','.cs','.uproject','.txt'}
def require(cond,msg):
    if not cond: errors.append(msg)
def is_skipped(p):
    return any(part in SKIP_PARTS for part in p.parts)
up=json.loads((ROOT/'METSE.uproject').read_text())
require(up.get('EngineAssociation')=='5.8','EngineAssociation must be 5.8')
require((ROOT/'Source/METSE/METSE.Build.cs').exists(),'Runtime module missing')
eng=(ROOT/'Config/DefaultEngine.ini').read_text(); game=(ROOT/'Config/DefaultGame.ini').read_text()
require('r.Nanite.ProjectEnabled=False' in eng,'Nanite must remain disabled for iPhone-first baseline')
require('r.DynamicGlobalIlluminationMethod=0' in eng,'Dynamic GI must remain disabled in baseline')
require('MaxCombatants=32' in game,'Combatant hard cap must remain 32')
require('TargetFPS=60' in game and 'ProtectionFPS=30' in game,'FPS guardrails missing')

ipa_workflow=ROOT/'.github/workflows/build-ios-unsigned.yml'
require(ipa_workflow.exists(),'Unsigned IPA workflow missing')
if ipa_workflow.exists():
    wf=ipa_workflow.read_text(errors='ignore')
    require('workflow_dispatch:' in wf,'iOS IPA build must remain explicitly dispatchable')
    require('\n  push:' not in wf and '\npush:' not in wf,'iOS IPA build must not run automatically on push')

update_cpp=(ROOT/'Source/METSE/Update/METSEUpdateCenterSubsystem.cpp'); require(update_cpp.exists(),'Update Center subsystem missing')
if update_cpp.exists():
    ut=update_cpp.read_text(errors='ignore')
    require('https://' in ut,'Update Center must enforce HTTPS')
    require('GetSHA256Signature' in ut,'Update Center must verify SHA-256')
    require('MountPak.IsBound()' in ut,'Update Center must fail closed when PAK mount is unavailable')
    require('MaxPackageBytes' in ut,'Update Center must have a package-size safety cap')
    require('requiresAppBuild' in ut and 'contentSchema' in ut,'Update compatibility gates missing')
for p in (ROOT/'Source').rglob('*'):
    if p.is_file() and not is_skipped(p) and p.suffix in {'.cpp','.h'}:
        txt=p.read_text(errors='ignore')
        if 'bCanEverTick=true' in txt.replace(' ','') and 'METSE_ALLOW_TICK' not in txt: errors.append(f'Unapproved tick enable: {p.relative_to(ROOT)}')
        if re.search(r'\bvoid\s+Tick\s*\(',txt) and 'METSE_ALLOW_TICK' not in txt: errors.append(f'Unapproved Tick override: {p.relative_to(ROOT)}')
for p in ROOT.rglob('*'):
    if not p.is_file() or is_skipped(p) or p.name in {'MANIFEST.sha256','guardrails.py'}: continue
    s=str(p.relative_to(ROOT))
    if s.endswith(('.p12','.mobileprovision')): errors.append(f'Signing secret must never be committed: {s}')
    if p.suffix.lower() in TEXT_SUFFIXES and p.stat().st_size<2_000_000:
        txt=p.read_text(errors='ignore')
        if 'BEGIN PRIVATE KEY' in txt or 'BEGIN RSA PRIVATE KEY' in txt: errors.append(f'Potential private key material: {s}')
if errors:
    print('METSE GUARDRAILS: FAIL'); [print(' -',e) for e in errors]; sys.exit(1)
print('METSE GUARDRAILS: PASS')
