#!/usr/bin/env python3
"""010-D event ownership and bounded witness checks; executable tests are required."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition, message):
    if not condition:
        errors.append(message)


def code(source):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)


def body(source, signature):
    begin = source.index("{", source.index(signature))
    depth, end = 1, begin + 1
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin + 1:end - 1]


h = code(read("Engine/Core/METSETacticalAICore.hpp"))
cpp = code(read("Engine/Core/METSETacticalAICore.cpp"))
engine = code(read("Engine/Core/METSEEngineCore.cpp"))
observe = body(cpp, "bool TacticalAICore::observeDamageResult")
require("const DamageResult& result" in cpp, "AI must consume immutable DamageCore event")
require("result.sequence!=lastObservedDamageSequence_+1" in observe, "damage ordering guard missing")
require("kMaxLossChecksPerStep = 128" in h and "lossChecksThisStep_>=kMaxLossChecksPerStep" in observe,
        "witness budget must be hard capped")
for token in ("TargetRelation::Friendly", "horizontalFovRadians", "kLossWitnessRangeMeters", "world.raycastSegment(eye,observed)"):
    require(token in observe, f"missing local witness truth: {token}")
for token in ("lastKnownPlayerPosition", "perceptionSource", "applyIntersection", "requestReload", "std::vector", "std::deque"):
    require(token not in observe, f"reaction observer crosses ownership: {token}")
require(re.search(r"\bhealth01\s*=", observe) is None, "AI observer must not mutate health")
require(re.search(r"\bnew\s+|malloc\(|realloc\(|push_back\(", cpp + h) is None, "unbounded reaction allocation")
for signature in ("bool TacticalAICore::authorizeFire", "std::size_t TacticalAICore::fireAuthorizedShots"):
    require("injuryRecoveryRemaining>0.0" in body(cpp, signature), "injury recovery bypass")
require("tacticalAI_.observeDamageResult(result,combatants_,world_)" in engine, "ledger observer not integrated")
require("ai.lastObservedDamageSequence!=damage_.resultSequence()" in engine, "ledger mirror invariant missing")
require("tacticalAI_=aiCheckpoint" in engine and "agent.lastWitnessedLossCorrelationId" in engine,
        "reaction rollback/hash missing")
for path in ("Engine/Platform/Apple/METSEEngineBridge.mm", "iOS/METSE/ObservatoryViewController.swift"):
    presentation = code(read(path))
    require("aiObservedDamageSequence" in presentation and "aiLossBudgetDrops" in presentation, "missing reaction telemetry")
    require("observeDamageResult" not in presentation, "presentation must not drive reactions")
tests = read("Tests/TacticalInjuryTeamTests.cpp")
for name in ("injuryAndSequence", "witnessBoundariesAndExpiry", "boundedLossStorm", "realDamageRollbackAndReplay"):
    require(name + "();" in tests, f"missing executed test: {name}")
require("Tests/TacticalInjuryTeamTests.cpp" in read("Scripts/test_engine_core.sh"), "strict suite missing")
workflow = read(".github/workflows/build-ios-unsigned.yml")
require("python3 Scripts/guardrails_010d.py" in workflow and
        workflow.index("python3 Scripts/guardrails_010d.py") < workflow.index("bash Scripts/test_engine_core.sh"),
        "010-D guardrail must precede tests")
require(read("VERSION").strip() == "0.3.0" and read("BUILD").strip() == "8", "release seal not authorized")
if errors:
    print("METSE BUILD 010-D GUARDRAILS: FAIL\n" + "\n".join(errors))
    sys.exit(1)
print("METSE BUILD 010-D GUARDRAILS: PASS")
