#!/usr/bin/env python3
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []


def require(condition, message):
    if not condition:
        errors.append(message)


def text(path):
    file = ROOT / path
    return file.read_text(encoding="utf-8", errors="ignore") if file.exists() else ""


manifest_path = ROOT / "Content/asset_manifest.json"
mesh_path = ROOT / "Content/Assets/Weapons/M4A1/viewmodel.obj"
material_path = ROOT / "Content/Assets/Weapons/M4A1/viewmodel.mtl"
require(manifest_path.is_file(), "asset manifest missing")
require(mesh_path.is_file(), "native viewmodel OBJ missing")
require(material_path.is_file(), "native viewmodel materials missing")

if manifest_path.is_file():
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assets = manifest.get("assets", [])
    require(len(assets) == 1, "011-A must declare its exact viewmodel asset")
    if assets:
        asset = assets[0]
        require(asset.get("thirdPartyContent") is False, "viewmodel ownership must be explicit")
        require(asset.get("qualityTier") == "engineeringReference", "quality claim must remain honest")
        require(asset.get("generator") == "Tools/generate_viewmodel_asset.py", "reproducible generator missing")

mesh = text("Content/Assets/Weapons/M4A1/viewmodel.obj")
require(mesh.count("\nv ") >= 2500, "viewmodel geometric detail regressed below engineering floor")
require(mesh.count("\nf ") >= 700, "viewmodel face count regressed below engineering floor")
for material in ("gunmetal", "polymer", "handguard", "suppressor", "optic", "glass", "sleeve", "glove"):
    require(f"usemtl {material}" in mesh, f"viewmodel material group missing: {material}")

renderer = text("Engine/Platform/Apple/METSEViewmodelRenderer.mm")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
shader = text("Shaders/METSERenderer.metal")
project = text("project.yml")
workflow = text(".github/workflows/build-ios-unsigned.yml")
build = text("Scripts/build_unsigned_ipa.sh")

for token in ("ModelIO/ModelIO.h", "newMeshesFromAsset", "drawIndexedPrimitives", "METSEPerspective", "METSEColorForMaterial"):
    require(token in renderer, f"native mesh renderer contract missing: {token}")
for token in ("MTLPixelFormatDepth32Float", "METSEViewmodelRenderer", "encodeWithEncoder"):
    require(token in bridge, f"bridge viewmodel integration missing: {token}")
for token in ("metseMeshVertex", "metseMeshFragment", "MeshVertex", "baseColorMetallic"):
    require(token in shader, f"Metal mesh pipeline missing: {token}")
require("sdBox(wp" not in shader and "float2 wc=" not in shader,
        "legacy screen-space box weapon returned")
require("sdk: ModelIO.framework" in project, "iOS target must link ModelIO")
require("python3 Scripts/guardrails_011a.py" in workflow, "011-A guardrail missing from CI")
require("viewmodel.obj" in build and "asset_manifest.json" in build,
        "IPA build does not verify bundled native assets")

if errors:
    print("METSE BUILD 011-A NATIVE VISUAL GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 011-A NATIVE VISUAL GUARDRAILS: PASS")
