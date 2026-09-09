#!/usr/bin/env bash
set -euo pipefail
ARCHIVE_TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/metse-archive.XXXXXX")"
swiftc -warnings-as-errors -parse-as-library iOS/METSE/METSEDiagnosticArchive.swift Tests/DiagnosticArchiveTests.swift -o "$ARCHIVE_TEST_DIR/tests"
"$ARCHIVE_TEST_DIR/tests"
