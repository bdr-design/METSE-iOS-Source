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
    with plist_path.open('rb') as f:
        info = plistlib.load(f)
    req(info.get('UIDeviceFamily') == [1], 'Info.plist must be iPhone-only')
    req(info.get('UILaunchStoryboardName') == 'LaunchScreen', 'UILaunchStoryboardName must point to LaunchScreen')
    req('UILaunchScreen' not in info, 'Empty UILaunchScreen dictionary is forbidden; use compiled LaunchScreen.storyboard')
    req(info.get('UIRequiresFullScreen') is True, 'UIRequiresFullScreen must be true')
    req(info.get('UIStatusBarHidden') is True, 'UIStatusBarHidden must be true')
    req(info.get('UIViewControllerBasedStatusBarAppearance') is True, 'View-controller status bar appearance must be enabled')
    req(info.get('UISupportedInterfaceOrientations') == [
        'UIInterfaceOrientationLandscapeLeft',
        'UIInterfaceOrientationLandscapeRight'
    ], 'Only landscape orientations are allowed')

if launch_path.exists():
    try:
        launch_root = ET.parse(launch_path).getroot()
        req(launch_root.attrib.get('launchScreen') == 'YES', 'Storyboard must be marked launchScreen=YES')
        device = launch_root.find('device')
        req(device is not None and device.attrib.get('orientation') == 'landscape', 'Launch storyboard must declare landscape device orientation')
        req(device is not None and device.attrib.get('id') == 'retina6_12', 'Launch storyboard must target a modern iPhone reference device')
    except Exception as exc:
        errors.append(f'Launch storyboard XML invalid: {exc}')

proj = (ROOT / 'project.yml').read_text()
req('GENERATE_INFOPLIST_FILE: NO' in proj, 'Generated plist forbidden for screen-critical contract')
req('TARGETED_DEVICE_FAMILY: "1"' in proj, 'METSE must remain iPhone-only')
req('CURRENT_PROJECT_VERSION: "4"' in proj, 'Build must be 4')
req('MARKETING_VERSION: "0.1.2"' in proj, 'Version must be 0.1.2')

wf = (ROOT / '.github/workflows/build-ios-unsigned.yml').read_text()
req('runs-on: macos-15' in wf, 'Hosted macOS required')

build = (ROOT / 'Scripts/build_unsigned_ipa.sh').read_text()
req("p.get('UIDeviceFamily') == [1]" in build, 'Final IPA must validate iPhone-only family')
req("p.get('UILaunchStoryboardName') == 'LaunchScreen'" in build, 'Final IPA must validate LaunchScreen storyboard binding')
req("'UILaunchScreen' not in p" in build, 'Final IPA must reject UILaunchScreen fallback dictionary')
req("LaunchScreen.storyboardc" in build, 'Final app must validate compiled launch storyboard')
req('.ci-output' in build, 'CI outputs must be isolated from BUILD file')

core = '\n'.join(p.read_text(errors='ignore') for p in (ROOT / 'Engine/Core').glob('*') if p.is_file())
for forbidden in ('UIKit', 'MetalKit', 'Foundation/Foundation.h', 'MTLDevice', 'MTKView'):
    req(forbidden not in core, f'Portable core imports {forbidden}')

bootstrap = json.loads((ROOT / 'Content/bootstrap.json').read_text())
req(bootstrap.get('engineTuning', {}).get('maxCombatants') == 32, '32 combatant cap missing')

for p in ROOT.rglob('*'):
    if p.is_file() and p.suffix.lower() in {'.p12', '.mobileprovision', '.cer', '.ipa', '.xcarchive'}:
        errors.append(f'Forbidden artifact: {p.relative_to(ROOT)}')

if errors:
    print('METSE NATIVE GUARDRAILS: FAIL')
    for error in errors:
        print(' -', error)
    sys.exit(1)

print('METSE NATIVE GUARDRAILS: PASS')
