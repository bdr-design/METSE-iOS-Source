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
    with plist_path.open('rb') as f: info = plistlib.load(f)
    req(info.get('UIDeviceFamily') == [1], 'Info.plist must be iPhone-only')
    req(info.get('UILaunchStoryboardName') == 'LaunchScreen', 'UILaunchStoryboardName must point to LaunchScreen')
    req('UILaunchScreen' not in info, 'Empty UILaunchScreen dictionary is forbidden; use compiled LaunchScreen.storyboard')
    req(info.get('UIRequiresFullScreen') is True, 'UIRequiresFullScreen must be true')
    req(info.get('UIStatusBarHidden') is True, 'UIStatusBarHidden must be true')
    req(info.get('UIViewControllerBasedStatusBarAppearance') is True, 'View-controller status bar appearance must be enabled')
    req(info.get('UISupportedInterfaceOrientations') == ['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'], 'Only landscape orientations are allowed')
if launch_path.exists():
    try:
        launch_root = ET.parse(launch_path).getroot(); req(launch_root.attrib.get('launchScreen') == 'YES', 'Storyboard must be marked launchScreen=YES')
        device = launch_root.find('device'); req(device is not None and device.attrib.get('orientation') == 'landscape', 'Launch storyboard must declare landscape device orientation'); req(device is not None and device.attrib.get('id') == 'retina6_12', 'Launch storyboard must target a modern iPhone reference device')
    except Exception as exc: errors.append(f'Launch storyboard XML invalid: {exc}')
proj=(ROOT/'project.yml').read_text(); req('GENERATE_INFOPLIST_FILE: NO' in proj,'Generated plist forbidden for screen-critical contract'); req('TARGETED_DEVICE_FAMILY: "1"' in proj,'METSE must remain iPhone-only'); req('CURRENT_PROJECT_VERSION: "6"' in proj,'Build must be 6'); req('MARKETING_VERSION: "0.1.4"' in proj,'Version must be 0.1.4')
wf=(ROOT/'.github/workflows/build-ios-unsigned.yml').read_text(); req('runs-on: macos-15' in wf,'Hosted macOS required'); req('C++ character + engine + integrity tests' in wf,'Character/engine/integrity tests must remain mandatory')
build=(ROOT/'Scripts/build_unsigned_ipa.sh').read_text(); req("p.get('UIDeviceFamily') == [1]" in build,'Final IPA must validate iPhone-only family'); req("p.get('UILaunchStoryboardName') == 'LaunchScreen'" in build,'Final IPA must validate LaunchScreen binding'); req("'UILaunchScreen' not in p" in build,'Final IPA must reject UILaunchScreen fallback'); req('LaunchScreen.storyboardc' in build,'Final app must validate compiled launch storyboard'); req('METSEIntegrityCore.o' in build,'Runtime Integrity compile evidence required'); req('METSECharacterMotor.o' in build,'Character Motor compile evidence required'); req('.ci-output' in build,'CI outputs must be isolated from BUILD file')
required=[ROOT/'Engine/Core/METSEIntegrityCore.hpp',ROOT/'Engine/Core/METSEIntegrityCore.cpp',ROOT/'Engine/Core/METSECharacterMotor.hpp',ROOT/'Engine/Core/METSECharacterMotor.cpp']
for path in required: req(path.exists(),f'Required core source missing: {path.relative_to(ROOT)}')
engine_header=(ROOT/'Engine/Core/METSEEngineCore.hpp').read_text(errors='ignore'); character_header=(ROOT/'Engine/Core/METSECharacterMotor.hpp').read_text(errors='ignore') if required[2].exists() else ''; character_source=(ROOT/'Engine/Core/METSECharacterMotor.cpp').read_text(errors='ignore') if required[3].exists() else ''; integrity_header=(ROOT/'Engine/Core/METSEIntegrityCore.hpp').read_text(errors='ignore') if required[0].exists() else ''; integrity_source=(ROOT/'Engine/Core/METSEIntegrityCore.cpp').read_text(errors='ignore') if required[1].exists() else ''; tests=(ROOT/'Tests/EngineCoreTests.cpp').read_text(errors='ignore'); bridge=(ROOT/'Engine/Platform/Apple/METSEEngineBridge.mm').read_text(errors='ignore'); shader=(ROOT/'Shaders/METSERenderer.metal').read_text(errors='ignore')
req('IntegrityCore integrity_' in engine_header,'EngineCore must own one Runtime Integrity control plane'); req('CharacterMotor characterMotor_' in engine_header,'EngineCore must own one Character Motor'); req('executeAtomic(' in engine_header,'External mutations need atomic command path'); req('validateInvariants()' in engine_header,'Atomic path must verify invariants'); req('kBlackBoxCapacity = 720' in engine_header,'Black Box must remain 720 frames'); req('setSprintHeld' in engine_header and 'cycleStance' in engine_header,'Sprint/stance must enter through EngineCore')
req('CharacterStance' in character_header,'Character stance model missing'); req('groundAcceleration' in character_header and 'groundDeceleration' in character_header,'Acceleration/deceleration missing'); req('gravity' in character_header and 'grounded' in character_header,'Gravity/ground contact missing'); req('bodyYaw' in character_header and 'viewYawOffset' in character_header,'Body/view separation missing'); req('viewYawSoftLimit' in character_header and 'viewYawHardLimit' in character_header,'Camera yaw limits missing'); req('eyeHeightTransitionSpeed' in character_header,'Eye-height transition missing'); req('state_.velocityX = moveToward' in character_source,'Velocity acceleration path missing'); req('state_.velocityY -= config_.gravity' in character_source,'Gravity integration missing')
req('kCommandCapacity = 96' in integrity_header,'Command ledger must remain bounded'); req('kEventCapacity = 256' in integrity_header,'Event ledger must remain bounded'); req('Sha256Digest' in integrity_header and 'sha256(' in integrity_source,'Portable SHA-256 required'); req('verifyJournal()' in integrity_header and 'verifyJournal() const' in integrity_source,'Journal verification required'); req('commandsRolledBack' in integrity_header,'Rollback metrics required')
req('testOnlyExecuteInvariantViolation' in tests,'Rollback regression test missing'); req('testOnlySetAirborne' in tests,'Gravity regression test missing'); req('CharacterStance::Crouched' in tests and 'CharacterStance::Prone' in tests,'Stance coverage missing'); req('setSprintHeld(true)' in tests,'Sprint coverage missing'); req('quiet_NaN' in tests and '33' in tests,'Invalid-input coverage missing'); req('kBlackBoxCapacity' in tests and 'kEventCapacity' in tests,'Bounded-memory coverage missing'); req('#import <simd/simd.h>' in bridge,'Bridge must explicitly import SIMD'); req('s.cameraHeight' in bridge,'Bridge must use Character Motor camera height'); req('u.character.x+u.character.y' in shader,'Metal camera must use eye height and vertical position')
core='\n'.join(p.read_text(errors='ignore') for p in (ROOT/'Engine/Core').glob('*') if p.is_file())
for forbidden in ('UIKit','MetalKit','Foundation/Foundation.h','MTLDevice','MTKView'): req(forbidden not in core,f'Portable core imports {forbidden}')
for forbidden in ('WKWebView','JavaScriptCore','localStorage','requestAnimationFrame'): req(forbidden not in core,f'Portable core contains forbidden web dependency: {forbidden}')
bootstrap=json.loads((ROOT/'Content/bootstrap.json').read_text()); req(bootstrap.get('engineTuning',{}).get('maxCombatants')==32,'32 combatant cap missing')
for p in ROOT.rglob('*'):
    if p.is_file() and p.suffix.lower() in {'.p12','.mobileprovision','.cer','.ipa','.xcarchive'}: errors.append(f'Forbidden artifact: {p.relative_to(ROOT)}')
if errors:
    print('METSE NATIVE GUARDRAILS: FAIL'); [print(' -',e) for e in errors]; sys.exit(1)
print('METSE NATIVE GUARDRAILS: PASS')
