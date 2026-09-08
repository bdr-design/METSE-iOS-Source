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


def require(haystack: str, needle: str, label: str) -> None:
    if needle not in haystack:
        errors.append(f"missing contract: {label}")


world_h = text("Engine/Core/METSEWorldCollision.hpp")
world_cpp = text("Engine/Core/METSEWorldCollision.cpp")
shader = text("Shaders/METSERenderer.metal")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
tests = text("Tests/BattlefieldMapTests.cpp")
runner = text("Scripts/test_engine_core.sh")
workflow = text(".github/workflows/build-ios-unsigned.yml")

# One bounded authoritative world list.
require(world_h, "static constexpr std::size_t kLegacyObstacleCount=6;", "009-F legacy obstacle count")
require(world_h, "static constexpr std::size_t kMaxObstacles=20;", "009-F obstacle hard cap")
require(world_h, "static constexpr std::size_t kMaxCoverCandidates=kMaxObstacles*4;", "cover derives from obstacle cap")
require(world_cpp, "obstacleCount_=kMaxObstacles;", "all bounded battlefield obstacles are authoritative")

# First six indices are the compatibility anchors used by prior Aim/ballistics tests.
legacy = [
    "obstacles_[0]={6,0,12,10,2.8,17,WorldMaterial::Concrete};",
    "obstacles_[1]={-14,0,8,-12,2.2,24,WorldMaterial::Steel};",
    "obstacles_[2]={-4,0,24,3,3.4,31,WorldMaterial::Concrete};",
    "obstacles_[3]={14,0,-3,14.18,1.7,-1,WorldMaterial::Wood};",
    "obstacles_[4]={-20,0,-22,-11,3.0,-13,WorldMaterial::Concrete};",
    "obstacles_[5]={-2.8,1.34,6.0,2.8,1.65,10.5,WorldMaterial::Steel};",
]
for index, anchor in enumerate(legacy):
    require(world_cpp, anchor, f"legacy obstacle {index} identity")

# 009-F must exercise all Material SSOT entries through actual geometry.
for material in ("Concrete", "Steel", "Wood", "Brick", "Glass", "Soil", "Rock"):
    require(world_cpp, f"WorldMaterial::{material}", f"battlefield material {material}")

# Renderer ABI must match the simulation cap and Bridge must derive its CPU buffer
# from the C++ owner instead of hard-coding a second count.
require(shader, "constant uint kMaxObstacles=20;", "Metal obstacle ABI cap")
require(bridge, "kRenderObstacleCap = metse::WorldCollisionCore::kMaxObstacles", "Bridge derives render obstacle cap from World SSOT")
if re.search(r"kRenderObstacleCap\s*=\s*20", bridge):
    errors.append("Bridge must not duplicate the 20-obstacle literal")

# Mandatory regression content and strict-gate wiring.
for contract in (
    "static_assert(WorldCollisionCore::kMaxObstacles==20)",
    "WorldCollisionCore::kMaxCoverCandidates<256",
    "openLane",
    "urban",
    "industrial",
    "rocky",
    "berm",
    "seedThirtyTwoAgents",
    "decisionsExecuted<=",
    "Build 009-F Battlefield Map Tests: PASS",
):
    require(tests, contract, f"BattlefieldMapTests contract {contract}")

require(runner, "Tests/BattlefieldMapTests.cpp", "009-F suite compiled by strict C++ runner")
require(workflow, "python3 Scripts/guardrails_009f.py", "009-F guardrail runs in iOS CI")

if errors:
    print("BUILD 009-F GUARDRAILS: FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("BUILD 009-F GUARDRAILS: PASS")
