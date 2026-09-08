#!/usr/bin/env python3
from pathlib import Path
import json, plistlib, sys, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]; errors=[]
def req(c,m):
    if not c: errors.append(m)

req((ROOT/'VERSION').read_text().strip()=='0.3.0','VERSION must remain 0.3.0 until Build 009 release seal')
req((ROOT/'BUILD').read_text().strip()=='8','BUILD must remain 8 until Build 009 release seal')
req(not any(ROOT.glob('*.uproject')),'Unreal files forbidden')
plist=ROOT/'iOS/METSE/Info.plist'; launch=ROOT/'iOS/METSE/LaunchScreen.storyboard'
req(plist.exists(),'Info.plist missing'); req(launch.exists(),'LaunchScreen missing')
if plist.exists():
    with plist.open('rb') as f: info=plistlib.load(f)
    req(info.get('UIDeviceFamily')==[1],'iPhone-only family required')
    req(info.get('UILaunchStoryboardName')=='LaunchScreen','LaunchScreen binding required')
    req('UILaunchScreen' not in info,'UILaunchScreen fallback forbidden')
    req(info.get('UIRequiresFullScreen') is True,'Full screen required')
    req(info.get('UIStatusBarHidden') is True,'Status bar hidden required')
    req(info.get('UISupportedInterfaceOrientations')==['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'],'Landscape-only required')
if launch.exists():
    try:
        root=ET.parse(launch).getroot(); req(root.attrib.get('launchScreen')=='YES','Storyboard must be launch screen')
    except Exception as e: errors.append(f'Launch storyboard invalid: {e}')
proj=(ROOT/'project.yml').read_text(); req('MARKETING_VERSION: "0.3.0"' in proj,'project version must remain 0.3.0 during Build 009 development'); req('CURRENT_PROJECT_VERSION: "8"' in proj,'project build must remain 8 during Build 009 development'); req('TARGETED_DEVICE_FAMILY: "1"' in proj,'iPhone-only project required'); req('GENERATE_INFOPLIST_FILE: NO' in proj,'Generated plist forbidden')

required=[
'Engine/Core/METSEInputCommandQueue.hpp','Engine/Core/METSEInputCommandQueue.cpp','Engine/Core/METSECharacterMotor.hpp','Engine/Core/METSECharacterMotor.cpp','Engine/Core/METSEWeaponCore.hpp','Engine/Core/METSEWeaponCore.cpp','Engine/Core/METSEWorldCollision.hpp','Engine/Core/METSEWorldCollision.cpp','Engine/Core/METSEDamageCore.hpp','Engine/Core/METSEDamageCore.cpp','Engine/Core/METSEBallisticsCore.hpp','Engine/Core/METSEBallisticsCore.cpp','Engine/Core/METSEVisibilityCore.hpp','Engine/Core/METSEVisibilityCore.cpp','Engine/Core/METSEObservatoryCore.hpp','Engine/Core/METSEObservatoryCore.cpp','Engine/Core/METSEIntegrityCore.hpp','Engine/Core/METSEIntegrityCore.cpp','Engine/Core/METSETacticalAICore.hpp','Engine/Core/METSETacticalAICore.cpp','Engine/Core/METSEEngineCore.hpp','Engine/Core/METSEEngineCore.cpp','Engine/Platform/Apple/METSEEngineBridge.mm','Shaders/METSERenderer.metal','iOS/METSE/ObservatoryViewController.swift','Docs/BUILD009_TACTICAL_COMBAT_PLAN_AR.md']
for r in required: req((ROOT/r).exists(),f'Required combat source missing: {r}')
text=lambda r:(ROOT/r).read_text(errors='ignore') if (ROOT/r).exists() else ''
queue=text('Engine/Core/METSEInputCommandQueue.hpp')+text('Engine/Core/METSEInputCommandQueue.cpp')
engine=text('Engine/Core/METSEEngineCore.hpp')+text('Engine/Core/METSEEngineCore.cpp')
weapon=text('Engine/Core/METSEWeaponCore.hpp')+text('Engine/Core/METSEWeaponCore.cpp')
world=text('Engine/Core/METSEWorldCollision.hpp')+text('Engine/Core/METSEWorldCollision.cpp')
ball=text('Engine/Core/METSEBallisticsCore.hpp')+text('Engine/Core/METSEBallisticsCore.cpp')
damage=text('Engine/Core/METSEDamageCore.hpp')+text('Engine/Core/METSEDamageCore.cpp')
vis=text('Engine/Core/METSEVisibilityCore.hpp')+text('Engine/Core/METSEVisibilityCore.cpp')
obs=text('Engine/Core/METSEObservatoryCore.hpp')+text('Engine/Core/METSEObservatoryCore.cpp')
ai=text('Engine/Core/METSETacticalAICore.hpp')+text('Engine/Core/METSETacticalAICore.cpp')
bridge_h=text('Engine/Platform/Apple/METSEEngineBridge.h'); bridge=text('Engine/Platform/Apple/METSEEngineBridge.mm'); shader=text('Shaders/METSERenderer.metal'); tests=text('Tests/EngineCoreTests.cpp')
req('kCapacity = 64' in queue or 'kCapacity=64' in queue,'Input queue capacity must be 64')
req('coalesced' in queue and 'rejectedCritical' in queue and 'removeOldestCoalescible' in queue,'Queue coalescing/overflow policy missing')
req('Do not coalesce across a discrete-command ordering barrier' in queue,'Ordering barrier rationale missing')
req('drainInputQueue()' in engine and 'inputQueue_.pushMove' in engine and 'inputQueue_.pushFire' in engine,'Single-owner queued input path missing')
req('fixedStep()' in engine and 'ballistics_.fixedStep' in engine and 'weapon_.fixedStep' in engine,'Combat fixed-step ownership missing')
req('executeAtomic(' in engine and 'MutationCheckpoint' in engine,'Atomic discrete command path missing')
req('deterministicStateHash' in engine,'Deterministic state hash missing')
req('magazineSize = 30' in weapon or 'magazineSize=30' in weapon,'Weapon magazine baseline missing')
req('roundsPerMinute = 700' in weapon or 'roundsPerMinute=700' in weapon,'Weapon RPM baseline missing')
req('sightConvergenceMeters = 100' in weapon or 'sightConvergenceMeters=100' in weapon,'Height-over-bore convergence missing')
req('raycastSegment' in world and 'clearanceHeightAt' in world and 'WorldMaterial' in world,'3D world ray/clearance/material model missing')
req('kMaxProjectiles=128' in ball or 'kMaxProjectiles = 128' in ball,'Projectile cap must be 128')
req('applySegment' in damage and 'HitRegion' in damage and 'correlationId' in damage,'Anatomical damage/correlation missing')
req('kMaxEntities=32' in vis or 'kMaxEntities = 32' in vis,'Visibility cap must be 32')
req('onePercentLowFPS' in obs and 'pointOnePercentLowFPS' in obs and 'p99FrameMilliseconds' in obs,'Observatory percentiles missing')
req('kMaxAgents = 32' in ai or 'kMaxAgents=32' in ai,'Tactical AI agent cap must be 32')
req('raycastSegment' in ai and 'lastKnownPlayerPosition' in ai and 'memorySeconds' in ai,'Tactical AI LOS/memory contract missing')
req('heardPlayer' in ai and 'playerNoise01' in ai,'Tactical AI hearing contract missing')
req('AISquadOrder' in ai and 'AIAlertState' in ai,'Tactical AI state/squad contract missing')
req('NSProcessInfoThermalStateSerious' in bridge and 'preferredFramesPerSecond = target' in bridge,'Thermal presentation fallback missing')
req('_core.advance' in bridge and '_core.setMovementInput' in bridge,'Bridge simulation/input integration missing')
req('(nullable instancetype)initWithView' in bridge_h,'Bridge initializer must remain nullable because Metal initialization can fail')
req('out float' not in shader,'GLSL-style out parameters are forbidden in Metal Shading Language')
req('kRenderProjectileCap' in bridge and 'targetData' in bridge,'Renderer telemetry bridge missing')
req('ads' in shader and 'projectilePositions' in shader and 'targetData' in shader and 'weaponMask' in shader,'Weapon/ADS/projectile/target Metal rendering missing')
req('METSE Build 009 Tactical Combat Foundation Tests: PASS' in tests,'Build009 tactical test banner missing')
for token in ('rejectedCritical','deterministicStateHash','HitRegion::Head','onePercentLowFPS','low roof','quiet_NaN','no magical player knowledge','TacticalAICore'):
    req(token in tests,f'Regression coverage missing: {token}')

core='\n'.join(p.read_text(errors='ignore') for p in (ROOT/'Engine/Core').glob('*') if p.is_file())
for forbidden in ('UIKit','MetalKit','Foundation/Foundation.h','MTLDevice','MTKView','WKWebView','JavaScriptCore','localStorage','requestAnimationFrame'):
    req(forbidden not in core,f'Portable core imports forbidden dependency: {forbidden}')
bootstrap=json.loads((ROOT/'Content/bootstrap.json').read_text()); req(bootstrap.get('engineTuning',{}).get('maxCombatants')==32,'32 combatant cap missing')
for p in ROOT.rglob('*'):
    if p.is_file() and p.suffix.lower() in {'.p12','.mobileprovision','.cer','.ipa','.xcarchive'}: errors.append(f'Forbidden artifact committed: {p.relative_to(ROOT)}')
wf=text('.github/workflows/build-ios-unsigned.yml'); req('runs-on: macos-15' in wf,'Hosted macOS build required'); req('C++ mega combat foundation tests' in wf,'C++ combat tests must be mandatory'); req('Metal compile fast gate' in wf,'Metal compiler gate must be mandatory'); req('Xcode platform compile gate' in wf,'Full platform type/compile gate must be mandatory'); req('METSE-v0.3.0-build008-mega-combat-unsigned' in wf,'Current development artifact name missing')
build=text('Scripts/build_unsigned_ipa.sh');
for obj in ('METSEInputCommandQueue','METSEWeaponCore','METSEDamageCore','METSEBallisticsCore','METSEVisibilityCore','METSEObservatoryCore','METSEIntegrityCore'):
    req(obj in build,f'Build evidence missing for {obj}')
if errors:
    print('METSE BUILD 009 DEVELOPMENT GUARDRAILS: FAIL'); [print(' -',e) for e in errors]; sys.exit(1)
print('METSE BUILD 009 DEVELOPMENT GUARDRAILS: PASS')
