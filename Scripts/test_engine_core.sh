#!/usr/bin/env bash
set -euo pipefail
OUT=.ci-output/tests
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
"$CXX" -std=c++20 -DMETSE_TESTING -Wall -Wextra -Wpedantic -Werror \
  Engine/Core/METSECharacterMotor.cpp \
  Engine/Core/METSEIntegrityCore.cpp \
  Engine/Core/METSEEngineCore.cpp \
  Tests/EngineCoreTests.cpp \
  -o "$OUT/engine_core_tests"
"$OUT/engine_core_tests"
