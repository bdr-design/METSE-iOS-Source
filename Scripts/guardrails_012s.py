#!/usr/bin/env python3
"""Build 012-S: player Settings/Options screen guardrails."""
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


settings_model = text("iOS/METSE/METSESettings.swift")
settings_ui = text("iOS/METSE/METSESettingsViewController.swift")
game_vc = text("iOS/METSE/GameViewController.swift")
gateway_vc = text("iOS/METSE/GatewayViewController.swift")
doc = text("Docs/BUILD012_S_SETTINGS_AR.md")

req(settings_model != "", "METSESettings.swift missing")
req(settings_ui != "", "METSESettingsViewController.swift missing")
req(doc != "", "Build 012-S settings documentation missing")

# Settings must remain a thin, isolated persistence layer: it must NEVER import or
# reference EngineCore/gameplay state directly, since simulation state has exactly one
# owner (EngineCore) per project architecture. Settings are read BY the view layer.
for forbidden in ("import Metal", "EngineCore", "METSEEngineBridge", "_core."):
    req(forbidden not in settings_model, f"Settings model must stay UI/persistence-only, forbidden: {forbidden}")
req("UserDefaults" in settings_model, "Settings must persist via UserDefaults (no custom save format needed for scalar prefs)")

for prop in ("lookSensitivity", "adsSensitivity", "gyroscopeEnabled", "gyroscopeSensitivity", "leftHandedLayout", "hudOpacity"):
    req(f"static var {prop}" in settings_model, f"Settings property missing: {prop}")
req("resetToDefaults" in settings_model, "Settings must support resetting to defaults")
req("clamped(to:" in settings_model, "Settings values must be clamped to their declared ranges on write")

req("case .settings" in gateway_vc and "METSESettingsViewController()" in gateway_vc,
    "Gateway must route .settings to the real settings screen, not the placeholder")

req("METSESettings.leftHandedLayout" in game_vc, "GameViewController must read the handedness setting")
req("METSESettings.lookSensitivity" in game_vc and "METSESettings.adsSensitivity" in game_vc,
    "GameViewController must apply independent look/ADS sensitivity settings")
req("METSESettings.gyroscopeEnabled" in game_vc, "GameViewController must gate gyroscope input on the setting")
req("METSESettings.hudOpacity" in game_vc, "GameViewController must apply the HUD opacity setting")
req("isAiming" in game_vc, "GameViewController must track aim state to distinguish look vs ADS sensitivity")
req("import CoreMotion" in game_vc, "Gyroscope-assisted aim requires CoreMotion")
req("gesture.view" in game_vc, "Pan handlers must key off gesture.view so handedness can swap pads without duplicating logic")

if errors:
    print("METSE BUILD 012-S SETTINGS GUARDRAILS: FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)
print("METSE BUILD 012-S SETTINGS GUARDRAILS: PASS")
