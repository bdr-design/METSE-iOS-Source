#!/usr/bin/env bash
set -euo pipefail
OUT=.ci-output
rm -rf "$OUT" METSE.xcodeproj
mkdir -p "$OUT"
xcodegen generate --spec project.yml
xcodebuild -project METSE.xcodeproj -scheme METSE -configuration Release -sdk iphoneos -derivedDataPath "$OUT/DerivedData" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" build | tee "$OUT/xcodebuild.log"
APP="$(find "$OUT/DerivedData/Build/Products/Release-iphoneos" -maxdepth 1 -type d -name 'METSE.app' -print -quit)"
[[ -n "$APP" ]] || { echo "METSE.app not produced" >&2; exit 30; }
PLIST="$APP/Info.plist"
python3 - "$PLIST" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as f: p=plistlib.load(f)
assert p.get('CFBundleShortVersionString')=='0.3.0', p.get('CFBundleShortVersionString')
assert p.get('CFBundleVersion')=='8', p.get('CFBundleVersion')
assert p.get('UIDeviceFamily')==[1], p.get('UIDeviceFamily')
assert p.get('UILaunchStoryboardName')=='LaunchScreen'
assert 'UILaunchScreen' not in p
assert p.get('UIRequiresFullScreen') is True
assert p.get('UIStatusBarHidden') is True
assert p.get('UIViewControllerBasedStatusBarAppearance') is True
assert p.get('UISupportedInterfaceOrientations')==['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight']
print('Build 008 modern iPhone launch contract: PASS')
PY
LAUNCH="$(find "$APP" -type d -name 'LaunchScreen.storyboardc' -print -quit)"
[[ -n "$LAUNCH" ]] || { echo "Compiled LaunchScreen.storyboardc missing" >&2; exit 31; }
BINARY="$APP/METSE"; [[ -f "$BINARY" ]] || { echo "METSE executable missing" >&2; exit 32; }
for object in METSEIntegrityCore METSEInputCommandQueue METSECharacterMotor METSEWeaponCore METSEWorldCollision METSEMaterialCore METSEDamageCore METSEBallisticsCore METSEVisibilityCore METSEAudioFXCore METSEObservatoryCore METSETacticalAICore METSEEngineCore METSEAudioPresenter; do
  found="$(find "$OUT/DerivedData/Build/Intermediates.noindex" -type f -name "${object}.o" -print -quit)"
  [[ -n "$found" ]] || { echo "${object}.o missing from Release intermediates" >&2; exit 40; }
  echo "Compile evidence: PASS ${object}.o"
done
mkdir -p "$OUT/Payload"; cp -R "$APP" "$OUT/Payload/METSE.app"
(cd "$OUT" && zip -qry METSE_v0.3.0_build008_unsigned.ipa Payload)
unzip -t "$OUT/METSE_v0.3.0_build008_unsigned.ipa" >/dev/null
shasum -a 256 "$OUT/METSE_v0.3.0_build008_unsigned.ipa" > "$OUT/METSE_v0.3.0_build008_unsigned.ipa.sha256"
echo "Unsigned IPA Build 008 with Build 009 development systems: PASS"
