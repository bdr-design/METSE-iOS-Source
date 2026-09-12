#!/usr/bin/env python3
"""Build 012-A2: soldier body asset wired into METSECombatantRenderer.mm, with a
mandatory fallback to the procedural humanoid on any load failure."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []


def req(condition, message):
    if not condition:
        errors.append(message)


def text(path):
    file = ROOT / path
    return file.read_text(errors="ignore") if file.exists() else ""


renderer = text("Engine/Platform/Apple/METSECombatantRenderer.mm")
manifest_path = ROOT / "Content/asset_manifest.json"
asset_path = ROOT / "Content/Assets/Characters/Soldier/soldier_body_v1.usdz"

req(asset_path.is_file(), "soldier body usdz asset missing")
req("#import <ModelIO/ModelIO.h>" in renderer, "ModelIO import missing from combatant renderer")
req("LoadCombatantBodyAsset" in renderer, "asset loader function missing")
req("BuildCombatantMesh(vertices,indices);" in renderer,
    "procedural fallback mesh must still be built BEFORE attempting the asset load")

# The fallback contract: BuildCombatantMesh must run unconditionally, and the asset
# loader may only ever REPLACE vertices/indices on full success - never partially.
build_call_index = renderer.find("BuildCombatantMesh(vertices,indices);")
load_call_index = renderer.find("LoadCombatantBodyAsset(vertices,indices)")
req(build_call_index != -1 and load_call_index != -1 and build_call_index < load_call_index,
    "procedural mesh must be built before the asset load is attempted (fallback ordering)")

for guard in ("if (!assetURL) return false;", "if (asset.count == 0) return false;",
              "if (meshes.count == 0) return false;", "if (loadedVertices.empty() || loadedIndices.empty()) return false;"):
    req(guard in renderer, f"asset loader missing a required failure guard: {guard}")

req("dependencies:" in text("project.yml") and "ModelIO.framework" in text("project.yml"),
    "ModelIO.framework must be linked for USD asset loading")

if manifest_path.is_file():
    import json
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    body_assets = [a for a in manifest.get("assets", []) if a.get("id") == "metse.combatant.body.healed_rigged.v1"]
    req(len(body_assets) == 1, "combatant body manifest entry missing or duplicated")
    if body_assets:
        req("not yet been wired" not in body_assets[0].get("fallbackRule", ""),
            "manifest fallbackRule text must be updated now that the loader exists")

if errors:
    print("METSE BUILD 012-A2 RENDERER WIRING GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 012-A2 RENDERER WIRING GUARDRAILS: PASS")
