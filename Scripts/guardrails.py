#!/usr/bin/env python3
from pathlib import Path
import json
import plistlib
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
errors=[]

def req(condition,message):
    if not condition:
        errors.append(message)

def text(path):
    file=ROOT/path
    return file.read_text(errors='ignore') if file.exists() else ''

version_value=(ROOT/'VERSION').read_text().strip()
build_value=(ROOT/'BUILD').read_text().strip()
req(build_value.isdigit(),'BUILD must be a plain integer string')
project_text_for_version=(ROOT/'project.yml').read_text()
req(f'MARKETING_VERSION: "{version_value}"' in project_text_for_version,
    'project.yml MARKETING_VERSION must match VERSION file (single source of truth)')
req(f'CURRENT_PROJECT_VERSION: "{build_value}"' in project_text_for_version,
    'project.yml CURRENT_PROJECT_VERSION must match BUILD file (single source of truth)')

# Found the hard way (a real CI failure): a stale hardcoded version can hide in ANY
# file, not just files named guardrails_*.py. build_unsigned_ipa.sh hardcoded the
# plist assertion to a literal old version, and METSEEngineBridge.mm hardcoded the
# app's own runtime diagnostics dictionary to a literal old version - neither matched
# this naming pattern, so the original grep sweep during PR #20 missed both. Checking
# both explicitly now, plus a repo-wide sweep for the specific stale literal so this
# can never quietly reappear a fifth time.
build_script_text=text('Scripts/build_unsigned_ipa.sh')
req('EXPECTED_VERSION' in build_script_text and 'EXPECTED_BUILD' in build_script_text and
    "cat VERSION" in build_script_text and "cat BUILD" in build_script_text,
    'build_unsigned_ipa.sh must read expected version/build from VERSION/BUILD files, not hardcode them')
bridge_text=text('Engine/Platform/Apple/METSEEngineBridge.mm')
req('CFBundleShortVersionString' in bridge_text and 'CFBundleVersion' in bridge_text,
    'METSEEngineBridge.mm must read version/build from the bundle Info.plist at runtime, not hardcode them')
for stale_literal in ('0.3.0', "@\"8\""):
    for checked_path in ('Scripts/build_unsigned_ipa.sh', 'Engine/Platform/Apple/METSEEngineBridge.mm'):
        req(stale_literal not in text(checked_path), f'stale version literal {stale_literal!r} found in {checked_path}')

req(not any(ROOT.glob('*.uproject')),'Unreal files forbidden')

plist=ROOT/'iOS/METSE/Info.plist'
launch=ROOT/'iOS/METSE/LaunchScreen.storyboard'
req(plist.exists(),'Info.plist missing')
req(launch.exists(),'LaunchScreen missing')
if plist.exists():
    with plist.open('rb') as handle:
        info=plistlib.load(handle)
    req(info.get('UIDeviceFamily')==[1],'iPhone-only family required')
    req(info.get('UILaunchStoryboardName')=='LaunchScreen','LaunchScreen binding required')
    req('UILaunchScreen' not in info,'UILaunchScreen fallback forbidden')
    req(info.get('UIRequiresFullScreen') is True,'Full screen required')
    req(info.get('UIStatusBarHidden') is True,'Status bar hidden required')
    req(info.get('UISupportedInterfaceOrientations')==['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'],'Landscape-only required')
    # Learned from a real crash on device: CMMotionManager.startDeviceMotionUpdates
    # terminates the app immediately if this key is absent, and nothing in Xcode's
    # build step catches that - only a real device run does. Enforce it structurally
    # instead of relying on remembering it next time CoreMotion is touched.
    if 'CoreMotion' in text('iOS/METSE/GameViewController.swift'):
        req(bool(info.get('NSMotionUsageDescription')),
            'NSMotionUsageDescription required in Info.plist whenever CoreMotion is used - missing this crashes on first launch, not at compile time')
if launch.exists():
    try:
        root=ET.parse(launch).getroot()
        req(root.attrib.get('launchScreen')=='YES','Storyboard must be launch screen')
    except Exception as error:
        errors.append(f'Launch storyboard invalid: {error}')

project=text('project.yml')
req('MARKETING_VERSION:' in project,'project.yml must declare MARKETING_VERSION')
req('CURRENT_PROJECT_VERSION:' in project,'project.yml must declare CURRENT_PROJECT_VERSION')
req('TARGETED_DEVICE_FAMILY: "1"' in project,'iPhone-only project required')
req('GENERATE_INFOPLIST_FILE: NO' in project,'Generated plist forbidden')
req('sdk: AVFoundation.framework' in project,'Native audio target must link AVFoundation')

required=[
    'Engine/Core/METSEInputCommandQueue.hpp','Engine/Core/METSEInputCommandQueue.cpp',
    'Engine/Core/METSECharacterMotor.hpp','Engine/Core/METSECharacterMotor.cpp',
    'Engine/Core/METSECombatantCore.hpp','Engine/Core/METSECombatantCore.cpp',
    'Engine/Core/METSEWeaponCore.hpp','Engine/Core/METSEWeaponCore.cpp',
    'Engine/Core/METSEWorldCollision.hpp','Engine/Core/METSEWorldCollision.cpp',
    'Engine/Core/METSEMaterialCore.hpp','Engine/Core/METSEMaterialCore.cpp',
    'Engine/Core/METSEDamageCore.hpp','Engine/Core/METSEDamageCore.cpp',
    'Engine/Core/METSEBallisticsCore.hpp','Engine/Core/METSEBallisticsCore.cpp',
    'Engine/Core/METSEVisibilityCore.hpp','Engine/Core/METSEVisibilityCore.cpp',
    'Engine/Core/METSEAudioFXCore.hpp','Engine/Core/METSEAudioFXCore.cpp',
    'Engine/Core/METSEObservatoryCore.hpp','Engine/Core/METSEObservatoryCore.cpp',
    'Engine/Core/METSEIntegrityCore.hpp','Engine/Core/METSEIntegrityCore.cpp',
    'Engine/Core/METSETacticalAICore.hpp','Engine/Core/METSETacticalAICore.cpp',
    'Engine/Core/METSEEngineCore.hpp','Engine/Core/METSEEngineCore.cpp',
    'Engine/Platform/Apple/METSEEngineBridge.mm','Engine/Platform/Apple/METSEAudioPresenter.h',
    'Engine/Platform/Apple/METSEAudioPresenter.mm','Shaders/METSERenderer.metal',
    'iOS/METSE/ObservatoryViewController.swift','Docs/BUILD009_TACTICAL_COMBAT_PLAN_AR.md',
    'Tests/EngineCoreTests.cpp','Tests/BallisticsMaterialTests.cpp','Tests/DamageAnatomyTests.cpp',
    'Tests/AudioFXVisibilityTests.cpp','Docs/BUILD009_G_AUDIO_FX_VISIBILITY_AR.md',
    'Scripts/guardrails_009h.py','Docs/BUILD009_H_OBSERVATORY_BLACKBOX_AR.md',
    'Scripts/guardrails_010a.py','Docs/BUILD010_A_COMBATANT_AUTHORITY_AR.md',
    'Tests/CombatantAuthorityTests.cpp','Scripts/guardrails_010b.py',
    'Docs/BUILD010_B_COMBATANT_LIFECYCLE_AR.md','Tests/CombatantLifecycleTests.cpp'
]
for relative in required:
    req((ROOT/relative).exists(),f'Required combat source missing: {relative}')

queue=text('Engine/Core/METSEInputCommandQueue.hpp')+text('Engine/Core/METSEInputCommandQueue.cpp')
engine=text('Engine/Core/METSEEngineCore.hpp')+text('Engine/Core/METSEEngineCore.cpp')
weapon=text('Engine/Core/METSEWeaponCore.hpp')+text('Engine/Core/METSEWeaponCore.cpp')
world=text('Engine/Core/METSEWorldCollision.hpp')+text('Engine/Core/METSEWorldCollision.cpp')
material=text('Engine/Core/METSEMaterialCore.hpp')+text('Engine/Core/METSEMaterialCore.cpp')
ballistics=text('Engine/Core/METSEBallisticsCore.hpp')+text('Engine/Core/METSEBallisticsCore.cpp')
damage=text('Engine/Core/METSEDamageCore.hpp')+text('Engine/Core/METSEDamageCore.cpp')
visibility=text('Engine/Core/METSEVisibilityCore.hpp')+text('Engine/Core/METSEVisibilityCore.cpp')
audio_fx=text('Engine/Core/METSEAudioFXCore.hpp')+text('Engine/Core/METSEAudioFXCore.cpp')
observatory=text('Engine/Core/METSEObservatoryCore.hpp')+text('Engine/Core/METSEObservatoryCore.cpp')
ai=text('Engine/Core/METSETacticalAICore.hpp')+text('Engine/Core/METSETacticalAICore.cpp')
integrity=text('Engine/Core/METSEIntegrityCore.hpp')+text('Engine/Core/METSEIntegrityCore.cpp')
bridge_h=text('Engine/Platform/Apple/METSEEngineBridge.h')
bridge=text('Engine/Platform/Apple/METSEEngineBridge.mm')
audio_presenter=text('Engine/Platform/Apple/METSEAudioPresenter.h')+text('Engine/Platform/Apple/METSEAudioPresenter.mm')
shader=text('Shaders/METSERenderer.metal')
engine_tests=text('Tests/EngineCoreTests.cpp')
material_tests=text('Tests/BallisticsMaterialTests.cpp')
damage_tests=text('Tests/DamageAnatomyTests.cpp')
test_script=text('Scripts/test_engine_core.sh')

req('kCapacity = 64' in queue or 'kCapacity=64' in queue,'Input queue capacity must be 64')
req('coalesced' in queue and 'rejectedCritical' in queue and 'removeOldestCoalescible' in queue,'Queue coalescing/overflow policy missing')
req('Do not coalesce across a discrete-command ordering barrier' in queue,'Ordering barrier rationale missing')
req('drainInputQueue()' in engine and 'inputQueue_.pushMove' in engine and 'inputQueue_.pushFire' in engine,'Single-owner queued input path missing')
req('fixedStep()' in engine and 'ballistics_.fixedStep' in engine and 'damage_.fixedStep' in engine and 'weapon_.fixedStep' in engine,'Combat fixed-step ownership missing')
req('executeAtomic(' in engine and 'MutationCheckpoint' in engine and 'consumedDamageResultSequence' in engine,'Atomic command/damage-ledger checkpoint missing')
req('deterministicStateHash' in engine and 'target.bleedingPerSecond' in engine,'Deterministic state hash must include combat state')

req('magazineSize = 30' in weapon or 'magazineSize=30' in weapon,'Weapon magazine baseline missing')
req('roundsPerMinute = 700' in weapon or 'roundsPerMinute=700' in weapon,'Weapon RPM baseline missing')
req('sightConvergenceMeters = 100' in weapon or 'sightConvergenceMeters=100' in weapon,'Height-over-bore convergence missing')
req('sprintToFireSeconds' in weapon and 'sprintRecoveryRemaining' in weapon and 'ReloadKind' in weapon,'Weapon Handling V2 sprint/reload contract missing')
req('recoilPattern' in weapon and 'never silently changes projectile direction' in weapon,'Deterministic visual recoil / Aim Truth contract missing')

req('WorldMaterial' in world and 'raycastSegment' in world and 'clearanceHeightAt' in world and 'thicknessMeters' in world and 'exitPoint' in world,'3D material contact truth missing')
req('MaterialBallisticProfile' in material and 'penetrationThresholdJoules' in material and 'maxPenetrationThicknessMeters' in material and 'ricochetEligible' in material,'Material ballistic SSOT incomplete')
req('MaterialCore::ballistic' in ballistics,'Ballistics must consume material SSOT')
req('kMaxProjectiles=128' in ballistics or 'kMaxProjectiles = 128' in ballistics,'Projectile cap must be 128')
req('kMaxPenetrationsPerProjectile=2' in ballistics or 'kMaxPenetrationsPerProjectile = 2' in ballistics,'Penetration chain must remain bounded to 2')
req('kMaxRicochetsPerProjectile=1' in ballistics or 'kMaxRicochetsPerProjectile = 1' in ballistics,'Ricochet chain must remain bounded to 1')
req('kMaxContactsPerStep=4' in ballistics or 'kMaxContactsPerStep = 4' in ballistics,'Per-slice ballistic contact work must remain bounded')

for token in ('HitRegion','Neck','Thorax','Abdomen','Arm','Leg','ArmorZone','CombatState','Incapacitated','DamageCause','Bleeding'):
    req(token in damage,f'Anatomy contract missing: {token}')
req('kResultCapacity = 192' in damage or 'kResultCapacity=192' in damage,'Damage result ledger must remain bounded to 192')
req('kMaxBleedingPerSecond = 4.0' in damage or 'kMaxBleedingPerSecond=4.0' in damage,'Bleeding state must remain bounded')
req('resultBySequence' in damage and 'queueResult' in damage,'Per-result damage ledger missing')
req('helmetArmorJoules' in damage and 'torsoArmorJoules' in damage and 'armorAbsorbedJoules' in damage,'Explicit armor degradation state missing')
req('reactionDirection' in damage and 'combatCapable' in damage,'Reaction direction / AI combat-capability contract missing')
req('TargetIncapacitated' in integrity and 'TargetIncapacitated' in engine,'Correlated incapacitation journal event missing')
req('resultBySequence' in engine and 'DamageCore::kResultCapacity' in engine,'Engine must publish every bounded same-slice damage result')
req('DamageCore::combatCapable' in engine,'Incapacitated targets must not remain Tactical AI combatants')
req('CombatantCore' in engine and 'DamageSource' in damage and 'includePlayerTarget' in ballistics,
    'Build010 combatant/player provenance contract missing')
req('CombatantLifecycleState' in engine and 'syncLifecycle' in engine,
    'Build010-B combatant lifecycle synchronization missing')

req('kMaxEntities=32' in visibility or 'kMaxEntities = 32' in visibility,'Visibility cap must be 32')
req('kCueCapacity=64' in audio_fx and 'kFXCapacity=48' in audio_fx and 'fxDropped' in audio_fx,'Bounded Audio/FX contract missing')
req('onePercentLowFPS' in observatory and 'pointOnePercentLowFPS' in observatory and 'p99FrameMilliseconds' in observatory,'Observatory percentiles missing')
req('kMaxAgents = 32' in ai or 'kMaxAgents=32' in ai,'Tactical AI agent cap must be 32')
req('raycastSegment' in ai and 'lastKnownPlayerPosition' in ai and 'memorySeconds' in ai,'Tactical AI LOS/memory contract missing')
req('AIPerceptionSource' in ai and 'hearingEstimate' in ai and 'hearingMaxLocalizationErrorMeters' in ai,'Tactical AI perception provenance/localization uncertainty missing')
req('AISquadOrder' in ai and 'AIAlertState' in ai,'Tactical AI state/squad contract missing')
req('TacticalAICore tacticalAI_' in engine and
    'initializeTacticalAI()' in engine and
    'stepTacticalAI()' in engine and
    'syncTacticalAICombatState()' in engine and
    'mirrorTacticalPositionsToDamage()' in engine and
    'tacticalAI_.fixedStep' in engine and
    engine.find('stepTacticalAI();')>=0 and engine.find('ballistics_.fixedStep')>engine.find('stepTacticalAI();'),
    'Tactical AI must be simulation-owned and stepped before ballistic target tracing')

req('NSProcessInfoThermalStateSerious' in bridge and 'preferredFramesPerSecond = target' in bridge,'Thermal presentation fallback missing')
req('_core.advance' in bridge and '_core.setMovementInput' in bridge,'Bridge simulation/input integration missing')
req('METSEAudioPresenter' in bridge and 'AVAudioSourceNode' in audio_presenter,'Native audio presentation consumer missing')
req('(nullable instancetype)initWithView' in bridge_h,'Bridge initializer must remain nullable because Metal initialization can fail')
req('out float' not in shader,'GLSL-style out parameters are forbidden in Metal Shading Language')
req('kRenderProjectileCap' in bridge and 'targetData' in bridge and 'targetMeta' in bridge and 'fxData' in bridge,'Renderer telemetry bridge missing')
req('ads' in shader and 'projectilePositions' in shader and 'targetData' in shader and 'targetMeta' in shader and 'fxData' in shader and 'weaponMask' in shader,'Weapon/ADS/projectile/target/FX Metal rendering missing')

req('METSE Build 009 Tactical Combat Foundation Tests: PASS' in engine_tests,'Build009 tactical test banner missing')
req('METSE Build 009 Ballistics + Material Contact Truth Tests: PASS' in material_tests,'009-C regression banner missing')
req('METSE Build 009 Anatomy + Damage Ledger Tests: PASS' in damage_tests,'009-D regression banner missing')
for token in ('rejectedCritical','deterministicStateHash','HitRegion::Head','onePercentLowFPS','low roof','quiet_NaN','magical player knowledge','TacticalAICore','fireSprintRecovery','ReloadKind::Empty','MaterialCore::ballistic'):
    req(token in engine_tests,f'Core regression coverage missing: {token}')
for token in ('WorldMaterial::Glass','WorldMaterial::Brick','ricochets==1','terminalWorldImpacts'):
    req(token in material_tests,f'Material regression coverage missing: {token}')
for token in ('HitRegion::Neck','HitRegion::Arm','ArmorZone::Torso','CombatState::Incapacitated','DamageCause::Bleeding','resultBySequence','testOnlySpawnProjectile','EventKind::DamageApplied'):
    req(token in damage_tests,f'Anatomy regression coverage missing: {token}')
req('DamageAnatomyTests.cpp' in test_script and 'BallisticsMaterialTests.cpp' in test_script,'009-C/009-D test binaries must be mandatory')

portable_core='\n'.join(path.read_text(errors='ignore') for path in (ROOT/'Engine/Core').glob('*') if path.is_file())
for forbidden in ('UIKit','MetalKit','Foundation/Foundation.h','MTLDevice','MTKView','WKWebView','JavaScriptCore','localStorage','requestAnimationFrame'):
    req(forbidden not in portable_core,f'Portable core imports forbidden dependency: {forbidden}')

bootstrap=json.loads((ROOT/'Content/bootstrap.json').read_text())
req(bootstrap.get('engineTuning',{}).get('maxCombatants')==32,'32 combatant cap missing')
for path in ROOT.rglob('*'):
    if path.is_file() and path.suffix.lower() in {'.p12','.mobileprovision','.cer','.ipa','.xcarchive'}:
        errors.append(f'Forbidden artifact committed: {path.relative_to(ROOT)}')

workflow=text('.github/workflows/build-ios-unsigned.yml')
req('runs-on: macos-15' in workflow,'Hosted macOS build required')
req('C++ mega combat foundation tests' in workflow,'C++ combat tests must be mandatory')
req('python3 Scripts/guardrails_009h.py' in workflow,'009-H guardrail must be part of CI')
req('Metal compile fast gate' in workflow,'Metal compiler gate must be mandatory')
req('Xcode platform compile gate' in workflow,'Full platform type/compile gate must be mandatory')
expected_artifact_prefix=f'METSE-v{version_value}-build{build_value.zfill(3)}'
req(expected_artifact_prefix in workflow,
    f'Workflow artifact name must track VERSION/BUILD (expected prefix {expected_artifact_prefix})')

build=text('Scripts/build_unsigned_ipa.sh')
for obj in ('METSEInputCommandQueue','METSEWeaponCore','METSECombatantCore','METSEWorldCollision','METSEMaterialCore','METSEDamageCore','METSEBallisticsCore','METSEVisibilityCore','METSEAudioFXCore','METSEObservatoryCore','METSEIntegrityCore','METSETacticalAICore','METSEEngineCore','METSEAudioPresenter'):
    req(obj in build,f'Build evidence missing for {obj}')

if errors:
    print('METSE BUILD 009 DEVELOPMENT GUARDRAILS: FAIL')
    for error in errors:
        print(' -',error)
    sys.exit(1)
print('METSE BUILD 009 DEVELOPMENT GUARDRAILS: PASS')
