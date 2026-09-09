#!/usr/bin/env python3
"""010-E bounded diagnostic capture and honest load coverage contracts."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text(encoding="utf-8")
h = read("Engine/Core/METSEEngineCore.hpp")
bridge = read("Engine/Platform/Apple/METSEEngineBridge.mm")
assert "EngineCore(const EngineCore&)=delete" in h
assert "coreCopy" not in bridge
assert "_core.captureDiagnostics(capture)" in bridge
assert bridge.index("capture.finish()") > bridge.index("os_unfair_lock_unlock(&_coreLock)", bridge.index("_core.captureDiagnostics(capture)"))
assert "observatorySnapshot()" not in read("iOS/METSE/GameViewController.swift")
assert "sizeof(EngineDiagnosticsCapture)<=192*1024" in read("Tests/TelemetryCaptureTests.cpp")
assert "o.fullCombatantLoadSeconds<=0.0" in bridge
assert "git rev-parse --verify HEAD" in read("Scripts/build_unsigned_ipa.sh")
assert "Tests/TelemetryCaptureTests.cpp" in read("Scripts/test_engine_core.sh")
workflow = read(".github/workflows/build-ios-unsigned.yml")
assert workflow.index("Scripts/guardrails_010e.py") < workflow.index("Scripts/test_engine_core.sh")
assert "bash Scripts/test_engine_soak.sh 7200" in workflow
assert read("VERSION").strip()=="0.3.0" and read("BUILD").strip()=="8"
print("010-E telemetry/soak guardrails: PASS")
