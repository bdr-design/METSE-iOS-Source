#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    target = ROOT / path
    if not target.is_file():
        errors.append(f"missing required file: {path}")
        return ""
    return target.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


combatant_h = text("Engine/Core/METSECombatantCore.hpp")
combatant_cpp = text("Engine/Core/METSECombatantCore.cpp")
engine_h = text("Engine/Core/METSEEngineCore.hpp")
engine_cpp = text("Engine/Core/METSEEngineCore.cpp")
tests = text("Tests/CombatantLifecycleTests.cpp")
runner = text("Scripts/test_engine_core.sh")
workflow = text(".github/workflows/build-ios-unsigned.yml")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
ui = text("iOS/METSE/ObservatoryViewController.swift")
doc = text("Docs/BUILD010_B_COMBATANT_LIFECYCLE_AR.md")

require("CombatantLifecycleState" in combatant_h and
        "Active=0" in combatant_h and "Wounded" in combatant_h and
        "Incapacitated" in combatant_h and "Dead" in combatant_h and "Removed" in combatant_h,
        "bounded combatant lifecycle states missing")
require("CombatantLifecycleReport" in combatant_h and "std::uint32_t dead" in combatant_h,
        "lifecycle report must remain fixed-size")
require("syncLifecycle" in combatant_h + combatant_cpp and "report() const noexcept" in combatant_h,
        "lifecycle transition/report API missing")
require("lifecycleFor" in engine_cpp and "syncLifecycle" in engine_cpp,
        "Engine must derive lifecycle from DamageCore state")
require("record->lifecycle!=lifecycleFor" in engine_cpp,
        "lifecycle must be part of invariant validation")
require("static_cast<std::uint64_t>(combatant.lifecycle)" in engine_cpp,
        "lifecycle must be in deterministic state hash")
require("combatantLifecycle" in engine_h + engine_cpp and "combatantDead" in bridge,
        "lifecycle report must reach diagnostics/Bridge")
require("combatantIncapacitated" in ui and "combatantRemoved" in ui,
        "Observatory must expose lifecycle counters")
for forbidden in ("std::vector", "std::deque", "std::list", "push_back(", "emplace_back(", "malloc(", "calloc(", "realloc("):
    require(forbidden not in combatant_h + combatant_cpp,
            f"lifecycle hot path forbids unbounded allocation/container: {forbidden}")
require(re.search(r"\bnew\s+", combatant_h + combatant_cpp) is None,
        "lifecycle hot path forbids heap allocation")
for token in ("syncLifecycle", "report.wounded", "report.incapacitated", "report.dead",
              "METSE Build 010-B Combatant Lifecycle Tests: PASS"):
    require(token in tests, f"lifecycle regression coverage missing: {token}")
require("Tests/CombatantLifecycleTests.cpp" in runner,
        "lifecycle suite must run in strict C++ gate")
require("python3 Scripts/guardrails_010b.py" in workflow,
        "Build 010-B guardrail must run before strict C++")
require("Combatant Lifecycle" in doc and "rollback" in doc and "Removed" in doc,
        "Build 010-B lifecycle contract documentation missing")
if errors:
    print("METSE BUILD 010-B COMBATANT LIFECYCLE GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 010-B COMBATANT LIFECYCLE GUARDRAILS: PASS")
