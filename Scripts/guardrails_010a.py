#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    p = ROOT / path
    if not p.is_file():
        errors.append(f"missing required file: {path}")
        return ""
    return p.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


combatant_h = text("Engine/Core/METSECombatantCore.hpp")
combatant_cpp = text("Engine/Core/METSECombatantCore.cpp")
damage_h = text("Engine/Core/METSEDamageCore.hpp")
damage_cpp = text("Engine/Core/METSEDamageCore.cpp")
weapon_h = text("Engine/Core/METSEWeaponCore.hpp")
ballistics_h = text("Engine/Core/METSEBallisticsCore.hpp")
ballistics_cpp = text("Engine/Core/METSEBallisticsCore.cpp")
ai_h = text("Engine/Core/METSETacticalAICore.hpp")
ai_cpp = text("Engine/Core/METSETacticalAICore.cpp")
engine_h = text("Engine/Core/METSEEngineCore.hpp")
engine_cpp = text("Engine/Core/METSEEngineCore.cpp")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
ui = text("iOS/METSE/ObservatoryViewController.swift")
tests = text("Tests/CombatantAuthorityTests.cpp")
runner = text("Scripts/test_engine_core.sh")
workflow = text(".github/workflows/build-ios-unsigned.yml")
doc = text("Docs/BUILD010_A_COMBATANT_AUTHORITY_AR.md")

require("kMaxCombatants = 32" in combatant_h or "kMaxCombatants=32" in combatant_h,
        "CombatantCore must enforce the fixed 32-combatant cap")
for token in ("CombatantIdentity", "CombatantRole", "TargetRelation", "TargetingPolicy",
              "DamageSource", "canTarget", "relation", "validate"):
    require(token in combatant_h + combatant_cpp, f"Combatant authority contract missing: {token}")
require("std::array<CombatantRecord,kMaxCombatants>" in combatant_h,
        "Combatant records must be fixed-capacity")
for forbidden in ("std::vector", "std::deque", "std::list", "std::map", "std::unordered_",
                  "push_back(", "emplace_back(", "malloc(", "calloc(", "realloc("):
    require(forbidden not in combatant_h + combatant_cpp,
            f"Combatant hot path forbids unbounded allocation/container: {forbidden}")
require("kPlayerId" in combatant_h and "kPlayerTeam" in combatant_h and "kHostileTeam" in combatant_h,
        "Player/hostile identity constants missing")

for token in ("playerTarget_", "playerTargetEnabled_", "configurePlayerTarget",
              "syncPlayerTargetPosition", "includePlayerTarget", "kPlayerTargetIndex"):
    require(token in damage_h + damage_cpp + engine_cpp, f"Player DamageTarget contract missing: {token}")
require("DamageSource" in damage_cpp and "CombatantCore::canTarget" in damage_cpp,
        "DamageCore must enforce centralized target filtering")
require("friendlyFireDenials" in damage_h + damage_cpp + engine_h + engine_cpp,
        "Friendly-fire denials must remain observable and bounded")
require("if(next==CombatState::Dead) target.bleedingPerSecond=0.0" in damage_cpp,
        "Dead targets must not retain active bleeding state")

for token in ("sourceCombatantId", "sourceTeamId", "sourceFactionId", "targetingPolicy"):
    require(token in weapon_h + ballistics_h + ballistics_cpp,
            f"Projectile provenance missing: {token}")
require("damage.traceSegment(from,to,source)" in ballistics_cpp,
        "Ballistics must pass source provenance into DamageCore")
require("includePlayerTarget" in ballistics_cpp and "CombatantCore::kPlayerId" in ballistics_cpp,
        "AI-to-player target path must be explicit and identity filtered")

require("fireAuthorizedShots" in ai_h and "weapon.fire" in ai_cpp,
        "TacticalAI must emit accepted WeaponCore shots without owning DamageCore")
require("spawnAuthorizedAIShots" in engine_h and "spawnAuthorizedAIShots()" in engine_cpp and
        "ballistics_.spawn(shot)" in engine_cpp,
        "Engine must own the AI Weapon->Ballistics handoff")
require("initializeCombatantAuthority" in engine_cpp and "combatants_.validate()" in engine_cpp,
        "Engine CombatantCore authority/invariant integration missing")
require("CombatantCore combatants{}" in engine_h and "combatants_=cp.combatants" in engine_h,
        "CombatantCore must be included in atomic rollback checkpoints")
require("aiShotsFired" in engine_cpp and "aiTargetImpacts" in engine_cpp,
        "AI combat metrics missing from EngineCore")

for forbidden in ("magicDamagePlayer", "playerDamageTarget->health", "aiDamageDirect",
                  "DamageCore::setAIPosition", "std::vector", "std::deque"):
    require(forbidden not in engine_cpp + ai_cpp + damage_cpp,
            f"Forbidden shortcut/ownership pattern found: {forbidden}")
require("Tactical locomotion" in engine_cpp and "mirrorTacticalPositionsToDamage" in engine_cpp,
        "DamageCore must remain a one-way target mirror, not movement owner")

for token in ("CombatantCore", "AI shots", "friendly-fire", "player impacts",
              "METSE Build 010-A Combatant Authority + Player Damage Tests: PASS"):
    require(token in tests, f"Build 010-A regression coverage missing: {token}")
require("Tests/CombatantAuthorityTests.cpp" in runner and
        "Engine/Core/METSECombatantCore.cpp" in runner,
        "Build 010-A suite/source must run in strict C++ gate")
require("python3 Scripts/guardrails_010a.py" in workflow,
        "Build 010-A guardrail must run before strict C++ tests")
require("Combatant Authority" in doc and "Friendly" in doc and "rollback" in doc,
        "Build 010-A contract documentation missing")
require("combatantCount" in bridge and "aiTargetImpacts" in bridge and
        "friendlyFireDenials" in bridge and "playerHealth" in ui,
        "Observatory must expose Build 010-A combatant/player metrics")

for forbidden in ('VERSION = "0.4.0"', 'BUILD = "9"', 'MARKETING_VERSION: "0.4.0"',
                  'CURRENT_PROJECT_VERSION: "9"'):
    require(forbidden not in text("VERSION") + text("BUILD") + text("project.yml"),
            f"Build 009 seal must precede version/build change: {forbidden}")

portable = combatant_h + combatant_cpp + damage_h + damage_cpp + ballistics_h + ballistics_cpp + ai_h + ai_cpp
require(re.search(r"\bnew\s+", portable) is None, "Build 010 portable combat path forbids heap new")

if errors:
    print("METSE BUILD 010-A COMBATANT AUTHORITY GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 010-A COMBATANT AUTHORITY GUARDRAILS: PASS")
