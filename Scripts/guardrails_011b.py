#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []


def text(path):
    file = ROOT / path
    return file.read_text(encoding="utf-8", errors="ignore") if file.is_file() else ""


def require(condition, message):
    if not condition:
        errors.append(message)


header = text("Engine/Platform/Apple/METSEBattlefieldRenderer.h")
renderer = text("Engine/Platform/Apple/METSEBattlefieldRenderer.mm")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
shader = text("Shaders/METSERenderer.metal")
workflow = text(".github/workflows/build-ios-unsigned.yml")
build = text("Scripts/build_unsigned_ipa.sh")

for token in ("METSEBattlefieldObstacle", "commandBuffer:(id<MTLCommandBuffer>)commandBuffer",
              "obstacles:(const METSEBattlefieldObstacle *)obstacles",
              "cameraX:(float)cameraX", "adsAlpha:(float)adsAlpha"):
    require(token in header, f"battlefield adapter contract missing: {token}")
for token in ("kMaximumObstacleInstances = 20", "kFramesInFlight = 3",
              "BattlefieldInstanceGPU", "BattlefieldSceneGPU", "MTLResourceStorageModeShared",
              "drawIndexedPrimitives", "instanceCount:write", "2.0f * std::atan",
              "dispatch_semaphore_wait", "addCompletedHandler", "dispatch_semaphore_signal"):
    require(token in renderer, f"bounded instanced renderer missing: {token}")
require("1 + kMaximumObstacleInstances + kBoundaryInstanceCount" in renderer,
        "battlefield instance capacity must derive from its bounded parts")
require("_frameIndex = (_frameIndex + 1) % kFramesInFlight" in renderer,
        "triple buffer rotation missing")
require("commandBuffer:commandBuffer" in bridge,
        "bridge must tie battlefield buffer reuse to GPU command completion")

for token in ("source.minX", "source.minY", "source.minZ", "source.maxX", "source.maxY",
              "source.maxZ", "source.material", "battlefieldObstacles.data()",
              "metseReticleFragment", "setDepthStencilState:nil"):
    require(token in bridge, f"authoritative obstacle copy incomplete: {token}")
world_draw = bridge.find("[self.battlefieldRenderer encodeWithEncoder")
viewmodel_draw = bridge.find("[self.viewmodelRenderer encodeWithEncoder")
require(world_draw >= 0 and viewmodel_draw > world_draw,
        "battlefield must draw before the first-person viewmodel")
reticle_draw = bridge.find("[encoder setRenderPipelineState:self.overlayPipeline]", viewmodel_draw)
require(reticle_draw > viewmodel_draw, "reticle overlay must draw after world and viewmodel")

for token in ("metseBattlefieldVertex", "metseBattlefieldFragment", "BattlefieldInstance",
              "materialAndKind", "tintAndRoughness", "edgeAxes", "distance * scene.sunAndFog.w",
              "metseReticleFragment", "discard_fragment"):
    require(token in shader, f"battlefield Metal material path missing: {token}")
require("float groundT" not in shader and "rayBox(camera" not in shader,
        "legacy fragment-raycast battlefield must stay removed")
require("World surfaces are rendered by metseBattlefieldVertex" in shader,
        "sky pass ownership rationale missing")

require("python3 Scripts/guardrails_011b.py" in workflow,
        "Build 011-B guardrail missing from CI")
require("METSEBattlefieldRenderer" in build,
        "IPA build must verify battlefield renderer compilation")
require((ROOT / "Docs/BUILD011_B_NATIVE_BATTLEFIELD_AR.md").is_file(),
        "Build 011-B engineering record missing")

if errors:
    print("METSE BUILD 011-B NATIVE BATTLEFIELD GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 011-B NATIVE BATTLEFIELD GUARDRAILS: PASS")
