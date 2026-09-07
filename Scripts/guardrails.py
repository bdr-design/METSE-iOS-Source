#!/usr/bin/env python3
from pathlib import Path
import json
import plistlib
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
errors = []

def req(condition, message):
    if not condition:
        errors.append(message)

req(not any(ROOT.glob('*.uproject')), 'Unreal files forbidden')
plist_path = ROOT / 'iOS/METSE/Info.plist'
launch_path = ROOT / 'iOS/METSE/LaunchScreen.storyboard'
req(plist_path.exists(), 'Explicit iPhone Info.plist missing')
req(launch_path.exists(), 'LaunchScreen.storyboard missing')
if plist_path.exists():
    with plist_path.open('rb') as handle:
        info = plistlib.load(handle)
    req(info.get('UIDeviceFamily') == [1], 'Info.plist must be iPhone-only')
    req(info.get('UILaunchStoryboardName') == 'LaunchScreen', 'LaunchScreen binding missing')
    req('UILaunchScreen' not in info, 'UILaunchScreen fallback forbidden')
    req(info.get('UIRequiresFullScreen') is True, 'UIRequiresFullScreen must be true')
    req(info.get('UIStatusBarHidden') is True, 'UIStatusBarHidden must be true')
    req(info.get('UISupportedInterfaceOrientations') == ['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'], 'Only landscape orientations allowed')
if launch_path.exists():
    try:
        root = ET.parse(launch_path).getroot()
        device = root.find('device')
        req(root.attrib.get('launchScreen') == 'YES', 'Launch storyboard must be launch screen')
        req(device is not None and device.attrib.get('orientation') == 'landscape', 'Launch storyboard must be landscape')
        req(device is not None and device.attrib.get('id') == 'retina6_12', 'Modern iPhone launch reference required')
    except Exception as exc:
        errors.append(f'Launch storyboard invalid: {exc}')

project = (ROOT / 'project.yml').read_text()
req('CURRENT_PROJECT_VERSION: "7"' in project, 'Build must be 7')
req('MARKETING_VERSION: "0.2.0"' in project, 'Version must be 0.2.0')
req('GENERATE_INFOPLIST_FILE: NO' in project, 'Explicit plist required')
req('TARGETED_DEVICE_FAMILY: "1"' in project, 'iPhone-only target required')

workflow = (ROOT / '.github/workflows/build-ios-unsigned.yml').read_text()
req('runs-on: macos-15' in workflow, 'Hosted macOS required')
req('C++ world + observatory + engine + integrity tests' in workflow, 'Mandatory C++ gate missing')

required = [
    'Engine/Core/METSEIntegrityCore.hpp', 'Engine/Core/METSEIntegrityCore.cpp',
    'Engine/Core/METSECharacterMotor.hpp', 'Engine/Core/METSECharacterMotor.cpp',
    'Engine/Core/METSEWorldCollision.hpp', 'Engine/Core/METSEWorldCollision.cpp',
    'Engine/Core/METSEObservatoryCore.hpp', 'Engine/Core/METSEObservatoryCore.cpp',
    'iOS/METSE/ObservatoryViewController.swift'
]
for relative in required:
    req((ROOT / relative).exists(), f'Missing required source: {relative}')

engine_h = (ROOT / 'Engine/Core/METSEEngineCore.hpp').read_text(errors='ignore')
engine_c = (ROOT / 'Engine/Core/METSEEngineCore.cpp').read_text(errors='ignore')
char_h = (ROOT / 'Engine/Core/METSECharacterMotor.hpp').read_text(errors='ignore')
char_c = (ROOT / 'Engine/Core/METSECharacterMotor.cpp').read_text(errors='ignore')
world_h = (ROOT / 'Engine/Core/METSEWorldCollision.hpp').read_text(errors='ignore')
world_c = (ROOT / 'Engine/Core/METSEWorldCollision.cpp').read_text(errors='ignore')
obs_h = (ROOT / 'Engine/Core/METSEObservatoryCore.hpp').read_text(errors='ignore')
obs_c = (ROOT / 'Engine/Core/METSEObservatoryCore.cpp').read_text(errors='ignore')
integrity_h = (ROOT / 'Engine/Core/METSEIntegrityCore.hpp').read_text(errors='ignore')
bridge_h = (ROOT / 'Engine/Platform/Apple/METSEEngineBridge.h').read_text(errors='ignore')
bridge_c = (ROOT / 'Engine/Platform/Apple/METSEEngineBridge.mm').read_text(errors='ignore')
shader = (ROOT / 'Shaders/METSERenderer.metal').read_text(errors='ignore')
tests = (ROOT / 'Tests/EngineCoreTests.cpp').read_text(errors='ignore')
game = (ROOT / 'iOS/METSE/GameViewController.swift').read_text(errors='ignore')
obs_ui = (ROOT / 'iOS/METSE/ObservatoryViewController.swift').read_text(errors='ignore')
build = (ROOT / 'Scripts/build_unsigned_ipa.sh').read_text(errors='ignore')

req('IntegrityCore integrity_' in engine_h, 'Engine must own IntegrityCore')
req('CharacterMotor characterMotor_' in engine_h, 'Engine must own CharacterMotor')
req('WorldCollisionCore worldCollision_' in engine_h, 'Engine must own WorldCollisionCore')
req('ObservatoryCore observatory_' in engine_h, 'Engine must own ObservatoryCore')
req('executeAtomic(' in engine_h and 'validateInvariants()' in engine_h, 'Atomic mutation path required')
req('kBlackBoxCapacity = 720' in engine_h, 'Black Box must remain bounded')
req('worldCollision_.resolve' in engine_c, 'World collision integration missing')
req('observatory_.observe' in engine_c, 'Observatory feed missing')
req('enum class CharacterGait' in char_h, 'Gait model missing')
for token in ('walkSpeed','tacticalSpeed','jogSpeed','sprintSpeed','capsuleRadius','cameraBobY','cameraRoll','landingOffset'):
    req(token in char_h, f'Character contract missing {token}')
req('updateCameraFeel' in char_c and 'applyHorizontalCollision' in char_c, 'Character feel/collision correction missing')
req('kMaxObstacles = 6' in world_h and 'CollisionResult' in world_h, 'Bounded world collision contract missing')
req('nearestBoundary' in world_c and 'std::clamp' in world_c, 'Collision sliding resolver missing')
req('kFrameCapacity = 600' in obs_h, 'Observatory ring must remain bounded')
for token in ('p95FrameMilliseconds','catchUpClampedFrames','distanceTravelled','totalCollisionContacts'):
    req(token in obs_h, f'Observatory metric missing {token}')
req('std::sort' in obs_c, 'P95 calculation missing')
req('kCommandCapacity = 96' in integrity_h and 'kEventCapacity = 256' in integrity_h, 'Integrity ledgers must remain bounded')
req('observatorySnapshot' in bridge_h and 'observatoryReportText' in bridge_h, 'Observatory bridge API missing')
req('os_unfair_lock' in bridge_c, 'Bridge synchronization missing')
req('worldObstacles()' in bridge_c and 'renderCpuAverageMs' in bridge_c and 'drawableMisses' in bridge_c, 'Renderer/world telemetry missing')
req('rayBoxDistance' in shader and 'obstacles[kMaxObstacles]' in shader, 'Metal obstacle renderer missing')
req('uniforms.worldExtra.y' in shader, 'Camera roll presentation missing')
req('joystickBase' in game and 'joystickKnob' in game, 'Visible joystick missing')
req('ObservatoryViewController(engine:' in game, 'In-game observatory entry missing')
req('نسخ تقرير الرصد' in obs_ui and 'مشاركة التقرير' in obs_ui and 'thermalState' in obs_ui, 'Observatory UI/report export incomplete')
req('WorldCollisionCore world' in tests and 'ObservatoryCore observatory' in tests, 'World/observatory tests missing')
req('CharacterGait::Walk' in tests and 'CharacterGait::Tactical' in tests and 'CharacterGait::Sprint' in tests, 'Gait tests missing')
req('quiet_NaN' in tests and '33' in tests, 'Adversarial input tests missing')
for obj in ('METSEIntegrityCore.o','METSECharacterMotor.o','METSEWorldCollision.o','METSEObservatoryCore.o','default.metallib'):
    req(obj in build, f'Final build evidence missing: {obj}')

core = '\n'.join(p.read_text(errors='ignore') for p in (ROOT / 'Engine/Core').glob('*') if p.is_file())
for forbidden in ('UIKit','MetalKit','Foundation/Foundation.h','MTLDevice','MTKView','WKWebView','JavaScriptCore','requestAnimationFrame'):
    req(forbidden not in core, f'Portable core contains forbidden dependency: {forbidden}')

bootstrap = json.loads((ROOT / 'Content/bootstrap.json').read_text())
req(bootstrap.get('engineTuning', {}).get('maxCombatants') == 32, '32 combatant cap missing')
req(bootstrap.get('runtimeObservatory', {}).get('frameWindow') == 600, 'Observatory window contract missing')

for path in ROOT.rglob('*'):
    if path.is_file() and path.suffix.lower() in {'.p12','.mobileprovision','.cer','.ipa','.xcarchive'}:
        errors.append(f'Forbidden artifact: {path.relative_to(ROOT)}')

if errors:
    print('METSE NATIVE GUARDRAILS: FAIL')
    for error in errors:
        print(' -', error)
    sys.exit(1)
print('METSE NATIVE GUARDRAILS: PASS')
