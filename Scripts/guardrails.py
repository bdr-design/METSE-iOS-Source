#!/usr/bin/env python3
from pathlib import Path
import json, sys

ROOT = Path(__file__).resolve().parents[1]
errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

require(not any(ROOT.glob("*.uproject")), "Unreal project files are forbidden in Native Metal baseline")
require(not (ROOT / "Source").exists(), "Legacy Unreal Source directory must not exist")
require((ROOT / "project.yml").exists(), "XcodeGen project.yml missing")
require((ROOT / "Engine/Core/METSEEngineCore.hpp").exists(), "Portable Engine Core missing")
require((ROOT / "Engine/Platform/Apple/METSEEngineBridge.mm").exists(), "Apple engine adapter missing")
require((ROOT / "iOS/METSE/GatewayViewController.swift").exists(), "Game Gateway missing")
require((ROOT / "Shaders/METSERenderer.metal").exists(), "Metal shader missing")

project = (ROOT / "project.yml").read_text(encoding="utf-8")
workflow = (ROOT / ".github/workflows/build-ios-unsigned.yml").read_text(encoding="utf-8")
require('runs-on: macos-15' in workflow, "iOS workflow must use GitHub-hosted macos-15")
build_script = (ROOT / "Scripts/build_unsigned_ipa.sh").read_text(encoding="utf-8")
test_script = (ROOT / "Scripts/test_engine_core.sh").read_text(encoding="utf-8")
require("CODE_SIGNING_ALLOWED=NO" in build_script and "CODE_SIGNING_REQUIRED=NO" in build_script,
        "CI must build unsigned IPA")
require("SWIFT_OBJC_BRIDGING_HEADER" in project, "Swift/Objective-C++ engine bridge contract missing")
require(".ci-output" in build_script and ".ci-output" in test_script and ".ci-output" in workflow,
        "All CI outputs must use the dedicated .ci-output workspace")
require("build/" not in build_script and "build/" not in test_script and "build/" not in workflow,
        "Lowercase build/ is forbidden because it collides with BUILD on case-insensitive macOS filesystems")

core = "\n".join(p.read_text(errors="ignore") for p in (ROOT / "Engine/Core").glob("*") if p.is_file())
for forbidden in ("UIKit", "MetalKit", "Foundation/Foundation.h", "MTLDevice", "MTKView"):
    require(forbidden not in core, f"Portable Engine Core imports platform dependency: {forbidden}")

gateway = (ROOT / "iOS/METSE/GatewayViewController.swift").read_text(encoding="utf-8")
require("ابدأ جلسة تكتيكية" in gateway, "Primary gateway action missing")
require("WKWebView" not in gateway, "Gateway must remain native, not WebView")

bootstrap = json.loads((ROOT / "Content/bootstrap.json").read_text(encoding="utf-8"))
require(bootstrap.get("contentSchema") == 1, "Content schema must remain 1 for this baseline")
require(bootstrap.get("engineTuning", {}).get("maxCombatants") == 32, "Combatant hard cap must be 32")

for p in ROOT.rglob("*"):
    if not p.is_file():
        continue
    rel = p.relative_to(ROOT).as_posix()
    if rel.endswith((".p12", ".mobileprovision", ".cer", ".ipa", ".xcarchive")):
        errors.append(f"Signing/build artifact committed: {rel}")
    if p.stat().st_size < 2_000_000 and p.suffix.lower() in {".swift",".mm",".m",".h",".hpp",".cpp",".py",".sh",".yml",".yaml",".json",".md",".txt"}:
        text = p.read_text(errors="ignore")
        if rel != "Scripts/guardrails.py" and ("BEGIN PRIVATE KEY" in text or "BEGIN RSA PRIVATE KEY" in text):
            errors.append(f"Private key material detected: {rel}")

if errors:
    print("METSE NATIVE GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE NATIVE GUARDRAILS: PASS")
