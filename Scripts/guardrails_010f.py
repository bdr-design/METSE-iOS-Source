#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text(encoding="utf-8")
archive = read("iOS/METSE/METSEDiagnosticArchive.swift")
recorder = read("iOS/METSE/METSEDiagnosticRecorder.swift")
assert "byteCap = 128 * 1024" in archive and "data.count <= Self.byteCap" in archive
assert '"previous.json" : "current.json"' in archive and ".atomic" in archive
assert "completeFileProtectionUntilFirstUserAuthentication" in archive
assert "isExcludedFromBackup = true" in archive
assert "guard !busy, let record = pending" in recorder and "pending = nil; busy = true" in recorder
assert "not proof of crash or clean exit" in recorder
assert "if problem == nil { pump() }" in recorder
for token in ("EngineCore", "setMovement", "setActiveCombatants", "triggerFire", "coreLock"):
    assert token not in archive + recorder
workflow = read(".github/workflows/build-ios-unsigned.yml")
assert workflow.index("Scripts/guardrails_010f.py") < workflow.index("Scripts/test_engine_core.sh")
assert "bash Scripts/test_diagnostic_archive.sh" in workflow
assert read("VERSION").strip() == "0.3.0" and read("BUILD").strip() == "8"
print("010-F bounded diagnostics persistence guardrails: PASS")
