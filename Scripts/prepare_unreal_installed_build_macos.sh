#!/usr/bin/env bash
set -euo pipefail

: "${ENGINE_ARCHIVE_URL:?Missing ENGINE_ARCHIVE_URL GitHub Actions secret}"
: "${ENGINE_ARCHIVE_SHA256:?Missing ENGINE_ARCHIVE_SHA256 GitHub Actions secret}"

WORK_DIR="${RUNNER_TEMP:-/tmp}/metse-unreal"
ARCHIVE="$WORK_DIR/unreal-engine-archive"
EXTRACT_DIR="$WORK_DIR/installed"

rm -rf "$WORK_DIR"
mkdir -p "$EXTRACT_DIR"

case "$ENGINE_ARCHIVE_URL" in
  https://*) ;;
  *) echo "ENGINE_ARCHIVE_URL must use HTTPS" >&2; exit 20 ;;
esac

EXPECTED="$(printf '%s' "$ENGINE_ARCHIVE_SHA256" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
[[ "$EXPECTED" =~ ^[0-9a-f]{64}$ ]] || { echo "ENGINE_ARCHIVE_SHA256 must be exactly 64 hex characters" >&2; exit 21; }

echo "Downloading verified Unreal Installed Build..."
curl --fail --location --retry 3 --retry-delay 5 --connect-timeout 30 --output "$ARCHIVE" "$ENGINE_ARCHIVE_URL"

ACTUAL="$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')"
if [[ "$ACTUAL" != "$EXPECTED" ]]; then
  echo "Unreal archive SHA-256 mismatch" >&2
  echo "expected=$EXPECTED" >&2
  echo "actual=$ACTUAL" >&2
  exit 22
fi

echo "Unreal archive SHA-256: PASS"

MIME="$(file -b "$ARCHIVE")"
if [[ "$MIME" == *"Zip archive"* ]]; then
  unzip -q "$ARCHIVE" -d "$EXTRACT_DIR"
elif [[ "$MIME" == *"gzip compressed"* || "$MIME" == *"POSIX tar archive"* || "$MIME" == *"XZ compressed"* ]]; then
  tar -xf "$ARCHIVE" -C "$EXTRACT_DIR"
else
  echo "Unsupported Unreal archive format: $MIME" >&2
  exit 23
fi

RUNUAT="$(find "$EXTRACT_DIR" -type f -path '*/Engine/Build/BatchFiles/RunUAT.sh' -print -quit)"
[[ -n "$RUNUAT" ]] || { echo "RunUAT.sh not found; archive is not a valid Unreal Installed Build" >&2; exit 24; }

UE_ROOT="${RUNUAT%/Engine/Build/BatchFiles/RunUAT.sh}"
[[ -x "$RUNUAT" ]] || chmod +x "$RUNUAT"
[[ -d "$UE_ROOT/Engine" ]] || { echo "Derived UE_ROOT is invalid: $UE_ROOT" >&2; exit 25; }

EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
[[ -e "$EDITOR" ]] || { echo "UnrealEditor missing from Installed Build" >&2; exit 26; }

if [[ -n "${GITHUB_ENV:-}" ]]; then
  printf 'UE_ROOT=%s\n' "$UE_ROOT" >> "$GITHUB_ENV"
fi

printf 'UE_ROOT=%s\n' "$UE_ROOT"
echo "Unreal Installed Build preparation: PASS"
