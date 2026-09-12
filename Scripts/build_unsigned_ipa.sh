#!/usr/bin/env bash
set -euo pipefail
OUT=.ci-output
SOURCE_REVISION="$(git rev-parse --verify HEAD)"
rm -rf "$OUT" METSE.xcodeproj
mkdir -p "$OUT"
xcodegen generate --spec project.yml
xcodebuild -project METSE.xcodeproj -scheme METSE -configuration Release -sdk iphoneos -derivedDataPath "$OUT/DerivedData" METSE_SOURCE_COMMIT="$SOURCE_REVISION" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" build | tee "$OUT/xcodebuild.log"
APP="$(find "$OUT/DerivedData/Build/Products/Release-iphoneos" -maxdepth 1 -type d -name 'METSE.app' -print -quit)"
[[ -n "$APP" ]] || { echo "METSE.app not produced" >&2; exit 30; }
PLIST="$APP/Info.plist"
python3 - "$PLIST" "$SOURCE_REVISION" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as f: p=plistlib.load(f)
assert p.get('CFBundleShortVersionString')=='0.3.0', p.get('CFBundleShortVersionString')
assert p.get('CFBundleVersion')=='8', p.get('CFBundleVersion')
assert len(sys.argv[2])==40 and all(c in '0123456789abcdef' for c in sys.argv[2])
assert p.get('METSESourceCommit')==sys.argv[2], p.get('METSESourceCommit')
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
for object in METSEIntegrityCore METSEInputCommandQueue METSECharacterMotor METSECombatantCore METSEWeaponCore METSEWorldCollision METSEMaterialCore METSEDamageCore METSEBallisticsCore METSEVisibilityCore METSEAudioFXCore METSEObservatoryCore METSETacticalAICore METSEEngineCore METSEAudioPresenter METSEViewmodelRenderer; do
  found="$(find "$OUT/DerivedData/Build/Intermediates.noindex" -type f -name "${object}.o" -print -quit)"
  [[ -n "$found" ]] || { echo "${object}.o missing from Release intermediates" >&2; exit 40; }
  echo "Compile evidence: PASS ${object}.o"
done
VIEWMODEL_RESOURCE="$(find "$APP" -type f -name 'viewmodel.obj' -print -quit)"
ASSET_MANIFEST_RESOURCE="$(find "$APP" -type f -name 'asset_manifest.json' -print -quit)"
[[ -n "$VIEWMODEL_RESOURCE" ]] || { echo "viewmodel.obj missing from app bundle" >&2; exit 41; }
[[ -n "$ASSET_MANIFEST_RESOURCE" ]] || { echo "asset_manifest.json missing from app bundle" >&2; exit 42; }
mkdir -p "$OUT/Payload"; cp -R "$APP" "$OUT/Payload/METSE.app"
(cd "$OUT" && zip -qry METSE_v0.3.0_build008_unsigned.ipa Payload)
unzip -t "$OUT/METSE_v0.3.0_build008_unsigned.ipa" >/dev/null
shasum -a 256 "$OUT/METSE_v0.3.0_build008_unsigned.ipa" > "$OUT/METSE_v0.3.0_build008_unsigned.ipa.sha256"
echo "Unsigned IPA Build 008 with Build 009 development systems: PASS"
