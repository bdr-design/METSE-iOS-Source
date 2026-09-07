#!/usr/bin/env bash
set -euo pipefail

rm -rf build/DerivedData build/Payload build/METSE.app build/METSE_v0.1.0_build002_unsigned.ipa
mkdir -p build

xcodegen generate --spec project.yml

xcodebuild \
  -project METSE.xcodeproj \
  -scheme METSE \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath build/DerivedData \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build | tee build/xcodebuild.log

APP="$(find build/DerivedData/Build/Products/Release-iphoneos -maxdepth 1 -type d -name 'METSE.app' -print -quit)"
[[ -n "$APP" ]] || { echo "METSE.app not produced" >&2; exit 30; }

PLIST="$APP/Info.plist"
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$PLIST" | grep -qx '0.1.0'
/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$PLIST" | grep -qx '2'

mkdir -p build/Payload
cp -R "$APP" build/Payload/METSE.app
(
  cd build
  zip -qry METSE_v0.1.0_build002_unsigned.ipa Payload
)
unzip -t build/METSE_v0.1.0_build002_unsigned.ipa >/dev/null
shasum -a 256 build/METSE_v0.1.0_build002_unsigned.ipa > build/METSE_v0.1.0_build002_unsigned.ipa.sha256
echo "Unsigned IPA: PASS"
