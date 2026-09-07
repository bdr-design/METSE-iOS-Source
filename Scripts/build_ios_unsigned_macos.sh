#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${UE_ROOT:?UE_ROOT must point to Unreal Engine 5.8 root}"
UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
[[ -x "$UAT" ]] || { echo "RunUAT.sh missing: $UAT"; exit 2; }
mkdir -p "$ROOT/Artifacts" "$ROOT/BuildLogs"
HELP="$ROOT/BuildLogs/BuildCookRun-help.txt"
"$UAT" BuildCookRun -Help > "$HELP" 2>&1 || true
if ! grep -Eiq 'nocodesign|nosign' "$HELP"; then echo "No supported no-sign option advertised; refusing unsigned IPA creation." >&2; exit 41; fi
NOSIGN="-nocodesign"; if ! grep -Eiq 'nocodesign' "$HELP"; then NOSIGN="-nosign"; fi
"$UAT" BuildCookRun -project="$ROOT/METSE.uproject" -noP4 -platform=IOS -target=METSE -clientconfig=Shipping -build -cook -stage -pak -package -archive -archivedirectory="$ROOT/Artifacts/UEArchive" -unattended -utf8output "$NOSIGN" | tee "$ROOT/BuildLogs/iOS-BuildCookRun.log"
APP="$(find "$ROOT/Artifacts/UEArchive" "$ROOT/Saved/StagedBuilds" -type d -name '*.app' -print -quit 2>/dev/null || true)"
[[ -n "$APP" && -d "$APP" ]] || { echo "No .app produced; refusing IPA creation"; exit 42; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/Payload"; cp -R "$APP" "$TMP/Payload/"
find "$TMP/Payload" -name _CodeSignature -type d -prune -exec rm -rf {} + || true
find "$TMP/Payload" -name embedded.mobileprovision -type f -delete || true
(cd "$TMP" && /usr/bin/zip -qry "$ROOT/Artifacts/METSE_v0.1.0_build001_unsigned.ipa" Payload)
/usr/bin/shasum -a 256 "$ROOT/Artifacts/METSE_v0.1.0_build001_unsigned.ipa" > "$ROOT/Artifacts/METSE_v0.1.0_build001_unsigned.ipa.sha256"
echo "Unsigned IPA created only after successful Unreal build/package gates."
