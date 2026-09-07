#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${UE_ROOT:?UE_ROOT must point to Unreal Engine root}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
[[ -x "$EDITOR" ]] || { echo "UnrealEditor-Cmd not found: $EDITOR"; exit 2; }
"$EDITOR" "$ROOT/METSE.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests METSE.;Quit" -TestExit="Automation Test Queue Empty" -ReportExportPath="$ROOT/TestResults/Automation"
