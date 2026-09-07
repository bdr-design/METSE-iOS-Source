#!/usr/bin/env bash
set -euo pipefail

OUTPUT_ROOT=".ci-output"
DERIVED_DATA="$OUTPUT_ROOT/DerivedData"
PAYLOAD_DIR="$OUTPUT_ROOT/Payload"
IPA="$OUTPUT_ROOT/METSE_v0.1.0_build002_unsigned.ipa"
IPA_SHA="$IPA.sha256"
LOG="$OUTPUT_ROOT/xcodebuild.log"

rm -rf "$DERIVED_DATA" "$PAYLOAD_DIR" "$IPA" "$IPA_SHA" "$LOG"
mkdir -p "$OUTPUT_ROOT"

xcodegen generate --spec project.yml

xcodebuild \
  -project METSE.xcodeproj \
  -scheme METSE \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath "$DERIVED_DATA" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build | tee "$LOG"

APP="$(find "$DERIVED_DATA/Build/Products/Release-iphoneos" -maxdepth 1 -type d -name 'METSE.app' -print -quit)"
[[ -n "$APP" ]] || { echo "METSE.app not produced" >&2; exit 30; }

PLIST="$APP/Info.plist"
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$PLIST" | grep -qx '0.1.0'
/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$PLIST" | grep -qx '2'

mkdir -p "$PAYLOAD_DIR"
cp -R "$APP" "$PAYLOAD_DIR/METSE.app"
(
  cd "$OUTPUT_ROOT"
  zip -qry "$(basename "$IPA")" Payload
)

unzip -t "$IPA" >/dev/null
shasum -a 256 "$IPA" > "$IPA_SHA"
echo "Unsigned IPA: PASS"
