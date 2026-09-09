#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []

def text(path: str) -> str:
    p = ROOT / path
    if not p.is_file():
        errors.append(f"missing file: {path}")
        return ""
    return p.read_text(encoding="utf-8")

engine_h = text("Engine/Core/METSEEngineCore.hpp")
engine_cpp = text("Engine/Core/METSEEngineCore.cpp")
obs_h = text("Engine/Core/METSEObservatoryCore.hpp")
obs_cpp = text("Engine/Core/METSEObservatoryCore.cpp")
tests = text("Tests/EngineCoreTests.cpp")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
ui = text("iOS/METSE/ObservatoryViewController.swift")
runner = text("Scripts/test_engine_core.sh")
doc = text("Docs/BUILD009_H_OBSERVATORY_BLACKBOX_AR.md")
reference_doc = text("Docs/BUILD009_REFERENCE_BINARY_AUDIT_AR.md")

def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)

require("simulationSliceMilliseconds" in obs_h and "latestAIActiveAgents" in obs_h,
        "Observatory V4 slice/AI telemetry contract missing")
require("projectileTerminalContacts" in obs_h and "simulationSlicesOver20ms" in obs_cpp,
        "Observatory V4 projectile-chain telemetry missing")
require("std::array<BlackBoxFrame,kBlackBoxCapacity>" in engine_h and
        "preSpike" in engine_h and "simulationSliceMilliseconds" in engine_cpp,
        "rolling pre-spike Black Box V2 contract missing")
require("std::chrono::steady_clock" in engine_cpp,
        "simulation slice timing must use monotonic clock")
require("preSpikeDeltaThreshold" in engine_cpp and "expected 30 FPS" in engine_cpp,
        "pre-spike telemetry must not flag the expected 30 FPS fallback")
require("preSpikeReasonMask" in engine_h and "preSpikeCallbackFrames" in engine_cpp and
        "retainedPreSpikeSimulationFrames" in engine_cpp,
        "Black Box spike cause attribution is missing")
require("rejectedSamples" in obs_h and "retainedRealSeconds" in obs_cpp and
        "windowFramesOver20ms" in obs_cpp and "simulationTickRegressions" in obs_cpp,
        "Observatory session/window validity telemetry is missing")
require("coreLockWaitAverageMs" in bridge and "callbackGapMaxMs" in bridge,
        "Bridge Observatory lock/callback telemetry missing")
require("callbackGapsOverBudget" in bridge and "thermalFallbackFrames" in bridge and
        "diagnosticProblemMask" in bridge and "acceptanceCoverageMask" in bridge,
        "Bridge timing attribution and coverage report are missing")
require("handleMemoryWarning" in bridge and "memoryWarningEvents" in bridge and
        "lifecycleDidEnterBackground" in bridge and "lifecycleDidBecomeActive" in bridge,
        "Bridge memory-pressure and typed lifecycle telemetry is missing")
require("Problems / Coverage" in ui and "telemetryRejectedSamples" in ui and
        "THERMAL FALLBACK" in ui and "memoryWarningEvents" in ui,
        "Observatory UI must expose problem/coverage attribution")
require("METSE OBSERVATORY V4 / BUILD009-H" in bridge,
        "Bridge Observatory report must identify the 009-H schema")
for token in ("maxSimulationSliceMilliseconds", "simulationSlicesOver20ms", "projectileContacts", "aiDecisions", "preSpike", "windowFramesOver20ms", "preSpikeReasonMask", "rejectedSamples"):
    require(token in tests, f"009-H regression coverage missing: {token}")
require("Tests/EngineCoreTests.cpp" in runner, "009-H observatory regression must run")
require("Observatory V4" in doc and "Black Box V2" in doc and "session/window" in doc and "سبب" in doc,
        "009-H documentation missing precision telemetry contract")
require("CydiaSubstrate" in reference_doc and "مصدر حقيقة" in reference_doc and
        "memory pressure" in reference_doc,
        "Reference binary audit must document the untrusted injection boundary and telemetry lesson")

if errors:
    print("BUILD 009-H OBSERVATORY/BLACK BOX GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("BUILD 009-H OBSERVATORY/BLACK BOX GUARDRAILS: PASS")
