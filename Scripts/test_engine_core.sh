#!/usr/bin/env bash
set -euo pipefail
CXX="${CXX:-clang++}"
OUT_DIR="${TMPDIR:-/tmp}/metse-build009-tests"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
COMMON=(
  Engine/Core/METSEInputCommandQueue.cpp
  Engine/Core/METSECharacterMotor.cpp
  Engine/Core/METSEWeaponCore.cpp
  Engine/Core/METSEWorldCollision.cpp
  Engine/Core/METSEMaterialCore.cpp
  Engine/Core/METSEDamageCore.cpp
  Engine/Core/METSEBallisticsCore.cpp
  Engine/Core/METSEVisibilityCore.cpp
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
