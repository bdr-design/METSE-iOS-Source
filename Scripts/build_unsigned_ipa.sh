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
import plistlib,sys
with open(sys.argv[1],'rb') as f:p=plistlib.load(f)
assert p.get('CFBundleShortVersionString')=='0.1.1',p.get('CFBundleShortVersionString')
assert p.get('CFBundleVersion')=='3',p.get('CFBundleVersion')
assert p.get('UIDeviceFamily')==[1],p.get('UIDeviceFamily')
launch=p.get('UILaunchScreen'); assert isinstance(launch,dict),launch
assert 'UILaunchScreen' not in launch,launch
assert p.get('UISupportedInterfaceOrientations')==['UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight']
print('Modern iPhone plist contract: PASS')
PY
mkdir -p "$OUT/Payload"; cp -R "$APP" "$OUT/Payload/METSE.app"
(cd "$OUT" && zip -qry METSE_v0.1.1_build003_unsigned.ipa Payload)
unzip -t "$OUT/METSE_v0.1.1_build003_unsigned.ipa" >/dev/null
shasum -a 256 "$OUT/METSE_v0.1.1_build003_unsigned.ipa" > "$OUT/METSE_v0.1.1_build003_unsigned.ipa.sha256"
echo "Unsigned IPA Build 003: PASS"
