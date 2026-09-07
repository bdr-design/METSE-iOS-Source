#!/usr/bin/env python3
from pathlib import Path
import json,sys
ROOT=Path(__file__).resolve().parents[1]; errors=[]
def req(c,m):
    if not c: errors.append(m)
req(not any(ROOT.glob('*.uproject')),'Unreal files forbidden')
req((ROOT/'iOS/METSE/Info.plist').exists(),'Explicit modern iPhone Info.plist missing')
proj=(ROOT/'project.yml').read_text(); req('GENERATE_INFOPLIST_FILE: NO' in proj,'Generated plist forbidden for screen-critical contract'); req('TARGETED_DEVICE_FAMILY: "1"' in proj,'METSE must remain iPhone-only'); req('CURRENT_PROJECT_VERSION: "3"' in proj,'Build must be 3')
wf=(ROOT/'.github/workflows/build-ios-unsigned.yml').read_text(); req('runs-on: macos-15' in wf,'Hosted macOS required')
build=(ROOT/'Scripts/build_unsigned_ipa.sh').read_text(); req("p.get('UIDeviceFamily')==[1]" in build,'Final IPA must validate iPhone-only family'); req("'UILaunchScreen' not in launch" in build,'Final IPA must reject nested launch screen'); req('.ci-output' in build,'CI outputs must be isolated from BUILD file')
core='\n'.join(p.read_text(errors='ignore') for p in (ROOT/'Engine/Core').glob('*') if p.is_file())
for f in ('UIKit','MetalKit','Foundation/Foundation.h','MTLDevice','MTKView'): req(f not in core,f'Portable core imports {f}')
bootstrap=json.loads((ROOT/'Content/bootstrap.json').read_text()); req(bootstrap.get('engineTuning',{}).get('maxCombatants')==32,'32 combatant cap missing')
for p in ROOT.rglob('*'):
    if p.is_file() and p.suffix.lower() in {'.p12','.mobileprovision','.cer','.ipa','.xcarchive'}: errors.append(f'Forbidden artifact: {p.relative_to(ROOT)}')
if errors:
    print('METSE NATIVE GUARDRAILS: FAIL'); [print(' -',e) for e in errors]; sys.exit(1)
print('METSE NATIVE GUARDRAILS: PASS')
