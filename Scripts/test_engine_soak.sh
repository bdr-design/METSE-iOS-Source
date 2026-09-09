#!/usr/bin/env bash
set -euo pipefail
SOAK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/metse-soak.XXXXXX")"
SOAK_SECONDS="${1:-7200}"
SOAK_MODE="${2:-aging}"
[[ "$SOAK_MODE" == aging || "$SOAK_MODE" == combat ]]
[[ "$SOAK_SECONDS" =~ ^[0-9]+$ ]] && (( SOAK_SECONDS >= 120 && SOAK_SECONDS <= 7200 ))
"${CXX:-c++}" -std=c++20 -DMETSE_TESTING -Wall -Wextra -Wpedantic -Werror -O2 Engine/Core/*.cpp Tests/EngineSoakTests.cpp -o "$SOAK_DIR/soak"
git rev-parse HEAD
"$SOAK_DIR/soak" "$SOAK_SECONDS" "$SOAK_MODE"
