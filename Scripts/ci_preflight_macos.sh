#!/usr/bin/env bash
set -euo pipefail
xcodebuild -version
xcrun --sdk iphoneos --show-sdk-version
python3 Scripts/verify_manifest.py
python3 Scripts/guardrails.py
: "${UE_ROOT:?Set UE_ROOT on the self-hosted macOS runner}"
[[ -d "$UE_ROOT" ]] || { echo "UE_ROOT is invalid"; exit 1; }
echo "macOS iOS preflight: PASS"
