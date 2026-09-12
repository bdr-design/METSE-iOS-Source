#!/usr/bin/env python3
"""010-C ownership guards supplement (do not replace) executable regressions."""
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
    start = source.find(signature)
    require(start >= 0, f"missing function {signature}")
    if start < 0:
        return ""
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while end < len(source) and depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    require(depth == 0, f"unbalanced function {signature}")
    return source[begin + 1:end - 1]


h = code(read("Engine/Core/METSETacticalAICore.hpp"))
cpp = code(read("Engine/Core/METSETacticalAICore.cpp"))
engine = code(read("Engine/Core/METSEEngineCore.cpp"))
observe = body(cpp, "void TacticalAICore::observeProjectileSegment")
perceive = body(cpp, "void TacticalAICore::perceiveAgent")
for token in ("std::vector", "std::deque", "std::list", "malloc(", "realloc(", "push_back("):
    require(token not in h + cpp, f"unbounded tactical hot path: {token}")
require(re.search(r"\bnew\s+", h + cpp) is None, "heap allocation in tactical hot path")
require(re.search(r"kMaxSuppressionChecksPerStep\s*=\s*256\s*;", h), "suppression cap changed")
require("suppressionChecksThisStep_>=kMaxSuppressionChecksPerStep" in observe, "missing drop-tail cap")
require("CombatantCore::relation" in observe and "TargetRelation::Hostile" in observe, "missing authority filter")
require("segment.traversed" in observe and "world.raycastSegment(closest,chest)" in observe, "missing traversal/world truth")
for token in ("lastKnownPlayerPosition", "perceptionSource", "applyIntersection", "setMovement", "WeaponCore"):
    require(token not in observe, f"suppression observer crosses ownership: {token}")
require("suppression01" not in perceive, "suppression must not grant perception")
for signature in ("bool TacticalAICore::authorizeFire", "std::size_t TacticalAICore::fireAuthorizedShots"):
    require("suppression01>=kSuppressionThreshold" in body(cpp, signature), "fire bypasses suppression")
require("const TacticalAICore aiCheckpoint=tacticalAI_" in engine and "tacticalAI_=aiCheckpoint" in engine,
        "suppression must rollback with TacticalAI")
require("agent.lastSuppressionCorrelationId" in engine and "suppressionReport.suppressionBudgetDrops" in engine,
        "suppression omitted from hash")
require("20*BallisticsCore::kMaxProjectiles" in engine and "64*TacticalAICore::kMaxAgents" in engine,
        "hash capacity must include saturated bounded pools")
for path in ("Engine/Platform/Apple/METSEEngineBridge.mm", "iOS/METSE/ObservatoryViewController.swift"):
    presentation = code(read(path))
    require("aiSuppressionBudgetDrops" in presentation, "missing presentation budget telemetry")
    require("observeProjectileSegment" not in presentation, "presentation must not drive suppression")
tests = read("Tests/TacticalSuppressionTests.cpp")
for name in ("boundariesAndKnowledge", "identityAndWalls", "terminationAndTrajectoryTruth",
             "budgetStressAndRollback", "saturatedHashSensitivity"):
    require(name + "();" in tests, f"missing executed regression: {name}")
require("Tests/TacticalSuppressionTests.cpp" in read("Scripts/test_engine_core.sh"), "suite not wired")
workflow = read(".github/workflows/build-ios-unsigned.yml")
require("ref: ${{ github.event.pull_request.head.sha || github.sha }}" in workflow,
        "PR gates must check out exact head SHA, not synthetic merge ref")
require("python3 Scripts/guardrails_010c.py" in workflow and
        workflow.index("python3 Scripts/guardrails_010c.py") < workflow.index("bash Scripts/test_engine_core.sh"),
        "010-C guardrail must precede strict tests")
require(read("VERSION").strip() and read("BUILD").strip().isdigit(), "VERSION/BUILD must be a valid non-empty seal")
if errors:
    print("METSE BUILD 010-C GUARDRAILS: FAIL\n" + "\n".join(errors))
    sys.exit(1)
print("METSE BUILD 010-C GUARDRAILS: PASS")
