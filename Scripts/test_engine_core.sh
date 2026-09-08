#!/usr/bin/env bash
set -euo pipefail
CXX="${CXX:-clang++}"
OUT="${TMPDIR:-/tmp}/metse-build008-tests"
"$CXX" -std=c++20 -DMETSE_TESTING -Wall -Wextra -Wpedantic -Werror \
  Engine/Core/METSEInputCommandQueue.cpp \
  Engine/Core/METSECharacterMotor.cpp \
  Engine/Core/METSEWeaponCore.cpp \
  Engine/Core/METSEWorldCollision.cpp \
  Engine/Core/METSEDamageCore.cpp \
  Engine/Core/METSEBallisticsCore.cpp \
  Engine/Core/METSEVisibilityCore.cpp \
  Engine/Core/METSEObservatoryCore.cpp \
  Engine/Core/METSEIntegrityCore.cpp \
  Engine/Core/METSEEngineCore.cpp \
  Tests/EngineCoreTests.cpp -o "$OUT"
"$OUT"
