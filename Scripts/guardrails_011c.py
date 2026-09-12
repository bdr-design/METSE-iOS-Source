#!/usr/bin/env python3
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def text(path): return (ROOT/path).read_text(encoding="utf-8")
def require(condition,message):
    if not condition: raise SystemExit(f"BUILD 011-C GUARDRAIL FAILED: {message}")

header=text("Engine/Platform/Apple/METSECombatantRenderer.h")
renderer=text("Engine/Platform/Apple/METSECombatantRenderer.mm")
bridge=text("Engine/Platform/Apple/METSEEngineBridge.mm")
shader=text("Shaders/METSERenderer.metal")
workflow=text(".github/workflows/build-ios-unsigned.yml")
build=text("Scripts/build_unsigned_ipa.sh")
docs=text("Docs/BUILD011_C_NATIVE_COMBATANTS_AR.md")

require("METSECombatantRenderState" in header and "facingYaw" in header and "visibilityTier" in header,
        "plain render contract missing")
require("kMaximumCombatants = 32" in renderer and "kFramesInFlight = 3" in renderer,
        "bounded combatant/triple-buffer caps missing")
require("dispatch_semaphore_wait" in renderer and "addCompletedHandler" in renderer,
        "GPU buffer lifetime guard missing")
require("BuildCombatantMesh" in renderer and "AddEllipsoid" in renderer and "AddCylinder" in renderer,
        "full-body indexed proxy mesh missing")
require("drawIndexedPrimitives" in renderer and "instanceCount:write" in renderer,
        "indexed instanced draw missing")
for token in ("damageTargets()","visibilityCore().entities()","tacticalAI().agents()",
              "target.id==visibility.id&&target.id==agent.id"):
    require(token in bridge,f"authoritative identity-aligned bridge missing {token}")
world=bridge.index("[self.battlefieldRenderer encodeWithEncoder:encoder")
people=bridge.index("[self.combatantRenderer encodeWithEncoder:encoder")
weapon=bridge.index("[self.viewmodelRenderer encodeWithEncoder:encoder",people)
overlay=bridge.index("[encoder setRenderPipelineState:self.overlayPipeline]",weapon)
require(world<people<weapon<overlay,"render order must be world, combatants, viewmodel, overlay")
require("metseCombatantVertex" in shader and "metseCombatantFragment" in shader,
        "native combatant shaders missing")
require("uint targetCount" not in shader and "float3 feet = projectWorld" not in shader,
        "legacy screen-space target silhouettes remain")
require("combatState>=2.0" in shader and "lineOfSight=input.state.w" in shader,
        "combat/visibility presentation missing")
require("guardrails_011c.py" in workflow,"CI does not run 011-C guardrail")
require("METSECombatantRenderer" in build,"IPA compile evidence omits combatant renderer")
require("ليست أصل الجندي النهائي" in docs and "هيكل عظمي" in docs,
        "quality limitation is not explicit")
print("METSE BUILD 011-C NATIVE COMBATANT GUARDRAILS: PASS")
