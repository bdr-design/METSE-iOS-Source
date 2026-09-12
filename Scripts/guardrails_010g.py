#!/usr/bin/env python3
from pathlib import Path
root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text(encoding="utf-8")
test = read("Tests/EngineSoakTests.cpp")
assert 'mode=="aging" || mode=="combat"' in test
assert 'combatRounds ? !capable(first.snapshot()) : tick%7200==0' in test
assert 'combinedFrames*100>=static_cast<std::uint64_t>(seconds)*60*90' in test
assert 'rounds>1 && aiShots>0 && playerImpacts>0' in test
assert 'verify(); // Reset must not erase evidence' in test
assert 'METSEDiagnosticRecorder.shared.beginGameplay()' in read('iOS/METSE/GameViewController.swift')
assert 'func beginGameplay()' in read('iOS/METSE/METSEDiagnosticRecorder.swift')
for forbidden in ('playerHealth=', 'damage_.', 'testOnlySetAirborne', 'fno-exceptions', 'NDEBUG'):
    assert forbidden not in test + read("Scripts/test_engine_soak.sh")
workflow = read(".github/workflows/build-ios-unsigned.yml")
assert 'bash Scripts/test_engine_soak.sh 7200\n' in workflow
assert 'bash Scripts/test_engine_soak.sh 7200 combat' in workflow
assert workflow.index('Scripts/guardrails_010g.py') < workflow.index('Scripts/test_engine_core.sh')
assert read("VERSION").strip() and read("BUILD").strip().isdigit()
print("010-G separate aging/combat pressure guardrails: PASS")
