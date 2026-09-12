#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def text(p): return (ROOT/p).read_text(encoding="utf-8")
def require(c,m):
    if not c: raise SystemExit(f"BUILD 011-D GUARDRAIL FAILED: {m}")

header=text("Engine/Platform/Apple/METSEBallisticFXRenderer.h")
renderer=text("Engine/Platform/Apple/METSEBallisticFXRenderer.mm")
bridge=text("Engine/Platform/Apple/METSEEngineBridge.mm")
shader=text("Shaders/METSERenderer.metal")
workflow=text(".github/workflows/build-ios-unsigned.yml")
build=text("Scripts/build_unsigned_ipa.sh")
docs=text("Docs/BUILD011_D_NATIVE_BALLISTIC_FX_AR.md")

require("METSEBallisticFXRenderState" in header and "velocityX" in header and "life01" in header,"render contract missing")
for token in ("kMaximumProjectiles=128","kMaximumFX=48","kMaximumInstances=kMaximumProjectiles+kMaximumFX","kFramesInFlight=3"):
    require(token in renderer,f"bounded capacity missing {token}")
require("dispatch_semaphore_wait" in renderer and "addCompletedHandler" in renderer,"GPU lifetime guard missing")
require("drawIndexedPrimitives" in renderer and "instanceCount:count" in renderer,"indexed instancing missing")
require("metseBallisticFXVertex" in shader and "metseBallisticFXFragment" in shader,"world FX shaders missing")
require("uint projectileCount" not in shader and "uint fxCount" not in shader,"legacy screen-space projectile/FX loops remain")
for token in ("projectile.velocity.x","effect.velocity.x","localMuzzle","nativeBallisticFXWrite"):
    require(token in bridge,f"authoritative bridge path missing {token}")
world=bridge.index("[self.battlefieldRenderer encodeWithEncoder:encoder")
people=bridge.index("[self.combatantRenderer encodeWithEncoder:encoder")
ballistics=bridge.index("[self.ballisticFXRenderer encodeWithEncoder:encoder")
weapon=bridge.index("[self.viewmodelRenderer encodeWithEncoder:encoder",ballistics)
overlay=bridge.index("[encoder setRenderPipelineState:self.overlayPipeline]",weapon)
require(world<people<ballistics<weapon<overlay,"render order must preserve world depth and first-person overlay")
require("guardrails_011d.py" in workflow,"CI guard missing")
require("METSEBallisticFXRenderer" in build,"IPA object evidence missing")
require("ليست بعد نظام جسيمات GPU احترافيًا" in docs and "decals" in docs,"quality limit missing")
print("METSE BUILD 011-D NATIVE BALLISTIC FX GUARDRAILS: PASS")
