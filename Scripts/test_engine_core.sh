#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/tests
clang++ -std=c++20 -Wall -Wextra -Werror \
  -IEngine/Core \
  Engine/Core/METSEEngineCore.cpp Tests/EngineCoreTests.cpp \
  -o build/tests/metse_engine_core_tests
./build/tests/metse_engine_core_tests
