#!/usr/bin/env python3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def text(p): return (ROOT/p).read_text(encoding="utf-8")
def require(c,m):
    if not c: raise SystemExit(f"BUILD 011-E GUARDRAIL FAILED: {m}")

header=text("Engine/Platform/Apple/METSEEngineBridge.h")
bridge=text("Engine/Platform/Apple/METSEEngineBridge.mm")
game=text("iOS/METSE/GameViewController.swift")
workflow=text(".github/workflows/build-ios-unsigned.yml")
docs=text("Docs/BUILD011_E_COMBAT_HUD_AR.md")

require("combatHUDSnapshot" in header and "combatHUDSnapshot" in bridge,"compact HUD contract missing")
for key in ('@"ammo"','@"reserveAmmo"','@"health"','@"stance"','@"reloading"','@"obstructed"','@"engagedAI"','@"presentationFPS"'):
    require(key in bridge,f"HUD field missing {key}")
hud=bridge[bridge.index("- (NSDictionary<NSString *, id> *)combatHUDSnapshot"):bridge.index("- (NSDictionary<NSString *, id> *)observatorySnapshot")]
require("_coreLock" in hud and "_telemetryLock" in hud,"HUD snapshot lock ownership missing")
require("captureDiagnostics" not in hud and "observatoryReportText" not in hud,"HUD path must remain lightweight")
require("engine?.combatHUDSnapshot()" in game and "engine?.statusString()" not in game,"game HUD still consumes diagnostic string")
require("withTimeInterval: 0.20" in game,"HUD cadence must be explicit")
require("safeAreaLayoutGuide" in game and "safeAreaInsets" in game,"safe-area layout missing")
require("joystickActive" in game and "joystickHomeCenter" in game and "viewDidLayoutSubviews" in game,"floating joystick lifecycle missing")
require("self.joystickBase.center = self.joystickHomeCenter" in game,"joystick does not return home")
require("setMoveForward(0, strafe: 0)" in game and "setSprintHeld(false)" in game and "setAimHeld(false)" in game,"exit input reset missing")
require("guardrails_011e.py" in workflow,"CI guard missing")
require("لا تضيف محرر توزيع" in docs and "أجهزة iPhone متعددة" in docs,"remaining UX validation is not explicit")
print("METSE BUILD 011-E COMBAT HUD GUARDRAILS: PASS")
