#!/usr/bin/env bash
set -euo pipefail
if [[ -z "${CXX:-}" ]]; then
  if command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
  else
    CXX="c++"
  fi
fi
OUT_DIR="${TMPDIR:-/tmp}/metse-build009-tests"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
COMMON=(
  Engine/Core/METSEInputCommandQueue.cpp
  Engine/Core/METSECharacterMotor.cpp
  Engine/Core/METSECombatantCore.cpp
  Engine/Core/METSEWeaponCore.cpp
  Engine/Core/METSEWorldCollision.cpp
  Engine/Core/METSEMaterialCore.cpp
  Engine/Core/METSEDamageCore.cpp
  Engine/Core/METSEBallisticsCore.cpp
  Engine/Core/METSEVisibilityCore.cpp
  Engine/Core/METSEAudioFXCore.cpp
  Engine/Core/METSEObservatoryCore.cpp
  Engine/Core/METSEIntegrityCore.cpp
  Engine/Core/METSETacticalAICore.cpp
  Engine/Core/METSEEngineCore.cpp
)
FLAGS=(-std=c++20 -DMETSE_TESTING -Wall -Wextra -Wpedantic -Werror)

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/EngineCoreTests.cpp -o "$OUT_DIR/engine-core-tests"
"$OUT_DIR/engine-core-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/BallisticsMaterialTests.cpp -o "$OUT_DIR/ballistics-material-tests"
"$OUT_DIR/ballistics-material-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/DamageAnatomyTests.cpp -o "$OUT_DIR/damage-anatomy-tests"
"$OUT_DIR/damage-anatomy-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/TacticalAIActionCoverTests.cpp -o "$OUT_DIR/tactical-ai-action-cover-tests"
"$OUT_DIR/tactical-ai-action-cover-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/BattlefieldMapTests.cpp -o "$OUT_DIR/battlefield-map-tests"
"$OUT_DIR/battlefield-map-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/AudioFXVisibilityTests.cpp -o "$OUT_DIR/audio-fx-visibility-tests"
"$OUT_DIR/audio-fx-visibility-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/CombatantAuthorityTests.cpp -o "$OUT_DIR/combatant-authority-tests"
"$OUT_DIR/combatant-authority-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/CombatantLifecycleTests.cpp -o "$OUT_DIR/combatant-lifecycle-tests"
"$OUT_DIR/combatant-lifecycle-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/TacticalSuppressionTests.cpp -o "$OUT_DIR/tactical-suppression-tests"
"$OUT_DIR/tactical-suppression-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/TacticalInjuryTeamTests.cpp -o "$OUT_DIR/tactical-injury-team-tests"
"$OUT_DIR/tactical-injury-team-tests"

"$CXX" "${FLAGS[@]}" "${COMMON[@]}" Tests/TelemetryCaptureTests.cpp -o "$OUT_DIR/telemetry-capture-tests"
"$OUT_DIR/telemetry-capture-tests"
