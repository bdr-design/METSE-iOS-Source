#!/usr/bin/env bash
set -euo pipefail
OUT=.ci-output
rm -rf "$OUT" METSE.xcodeproj
mkdir -p "$OUT"

xcodegen generate --spec project.yml
xcodebuild \
  -project METSE.xcodeproj \
  -scheme METSE \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath "$OUT/DerivedData" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build | tee "$OUT/xcodebuild.log"

APP="$(find "$OUT/DerivedData/Build/Products/Release-iphoneos" -maxdepth 1 -type d -name 'METSE.app' -print -quit)"
[[ -n "$APP" ]] || { echo "METSE.app not produced" >&2; exit 30; }
PLIST="$APP/Info.plist"

python3 - "$PLIST" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as f:
    p = plistlib.load(f)
assert p.get('CFBundleShortVersionString') == '0.1.4', p.get('CFBundleShortVersionString')
assert p.get('CFBundleVersion') == '6', p.get('CFBundleVersion')
assert p.get('UIDeviceFamily') == [1], p.get('UIDeviceFamily')
assert p.get('UILaunchStoryboardName') == 'LaunchScreen', p.get('UILaunchStoryboardName')
assert 'UILaunchScreen' not in p, p.get('UILaunchScreen')
assert p.get('UIRequiresFullScreen') is True, p.get('UIRequiresFullScreen')
assert p.get('UIStatusBarHidden') is True, p.get('UIStatusBarHidden')
assert p.get('UIViewControllerBasedStatusBarAppearance') is True, p.get('UIViewControllerBasedStatusBarAppearance')
assert p.get('UISupportedInterfaceOrientations') == [
    'UIInterfaceOrientationLandscapeLeft',
    'UIInterfaceOrientationLandscapeRight'
], p.get('UISupportedInterfaceOrientations')
print('Modern iPhone launch-screen contract: PASS')
PY

LAUNCH_COMPILED="$(find "$APP" -type d -name 'LaunchScreen.storyboardc' -print -quit)"
[[ -n "$LAUNCH_COMPILED" ]] || { echo "Compiled LaunchScreen.storyboardc missing from METSE.app" >&2; exit 31; }
[[ -f "$LAUNCH_COMPILED/Info.plist" || -f "$LAUNCH_COMPILED/UIViewController-01J-lp-oVM.nib" || -n "$(find "$LAUNCH_COMPILED" -type f -print -quit)" ]] || {
  echo "LaunchScreen.storyboardc is empty" >&2
  exit 32
}
echo "Compiled LaunchScreen: PASS ($LAUNCH_COMPILED)"

BINARY="$APP/METSE"
[[ -f "$BINARY" ]] || { echo "METSE executable missing" >&2; exit 33; }
INTEGRITY_OBJECT="$(find "$OUT/DerivedData/Build/Intermediates.noindex" -type f -name 'METSEIntegrityCore.o' -print -quit)"
[[ -n "$INTEGRITY_OBJECT" ]] || { echo "METSEIntegrityCore.o missing from Release intermediates" >&2; exit 34; }
echo "Runtime Integrity compile evidence: PASS ($INTEGRITY_OBJECT)"
CHARACTER_OBJECT="$(find "$OUT/DerivedData/Build/Intermediates.noindex" -type f -name 'METSECharacterMotor.o' -print -quit)"
[[ -n "$CHARACTER_OBJECT" ]] || { echo "METSECharacterMotor.o missing from Release intermediates" >&2; exit 35; }
echo "Character Motor compile evidence: PASS ($CHARACTER_OBJECT)"

mkdir -p "$OUT/Payload"
cp -R "$APP" "$OUT/Payload/METSE.app"
(cd "$OUT" && zip -qry METSE_v0.1.4_build006_unsigned.ipa Payload)
unzip -t "$OUT/METSE_v0.1.4_build006_unsigned.ipa" >/dev/null
shasum -a 256 "$OUT/METSE_v0.1.4_build006_unsigned.ipa" > "$OUT/METSE_v0.1.4_build006_unsigned.ipa.sha256"
echo "Unsigned IPA Build 006: PASS"
