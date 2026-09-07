#!/usr/bin/env bash
set -euo pipefail

OUTPUT_ROOT=".ci-output"
TEST_DIR="$OUTPUT_ROOT/tests"

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

clang++ -std=c++20 -Wall -Wextra -Werror \
  -IEngine/Core \
  Engine/Core/METSEEngineCore.cpp Tests/EngineCoreTests.cpp \
  -o "$TEST_DIR/metse_engine_core_tests"

"$TEST_DIR/metse_engine_core_tests"
