#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
errors=[]

def req(condition,message):
    if not condition:
        errors.append(message)

def text(path):
    file=ROOT/path
    return file.read_text(errors='ignore') if file.exists() else ''

world=text('Engine/Core/METSEWorldCollision.hpp')+text('Engine/Core/METSEWorldCollision.cpp')
damage=text('Engine/Core/METSEDamageCore.hpp')+text('Engine/Core/METSEDamageCore.cpp')
ai=text('Engine/Core/METSETacticalAICore.hpp')+text('Engine/Core/METSETacticalAICore.cpp')
engine=text('Engine/Core/METSEEngineCore.hpp')+text('Engine/Core/METSEEngineCore.cpp')
tests=text('Tests/TacticalAIActionCoverTests.cpp')
test_script=text('Scripts/test_engine_core.sh')
workflow=text('.github/workflows/build-ios-unsigned.yml')

req((ROOT/'Tests/TacticalAIActionCoverTests.cpp').exists(),'009-E regression suite missing')
req('TacticalAIActionCoverTests.cpp' in test_script,'009-E regression binary must be mandatory')
req('-Wall -Wextra -Wpedantic -Werror' in test_script,'Strict C++ warnings must remain mandatory')

# World-derived cover is the only cover truth. Do not permit a second authored/runtime SSOT.
req('WorldCoverCandidate' in world,'World-derived cover candidate contract missing')
req('kMaxCoverCandidates=kMaxObstacles*4' in world or 'kMaxCoverCandidates = kMaxObstacles*4' in world,'Cover candidate pool must remain bounded to obstacle geometry')
req('rebuildCoverCandidates()' in world,'Cover candidates must be derived from world collision geometry')
req('overhead-only geometry' in world.lower(),'Overhead-only cover rejection rationale missing')
req('coverCandidates()' in world and 'coverCandidateCount()' in world,'World cover accessors missing')

# TacticalAI owns locomotion; DamageCore is a one-way ballistic target mirror.
req('syncTargetPosition' in damage,'DamageCore tactical position mirror missing')
req('Tactical AI position remains authoritative' in ai,'Tactical locomotion ownership contract missing')
req('mirrorTacticalAIPositionsToDamage()' in engine,'Engine one-way AI->Damage mirror missing')
req('damage_.syncTargetPosition' in engine,'Engine must mirror TacticalAI transforms into DamageCore')
req('tacticalAI_.syncAgentCombatState' in engine,'Normal simulation must sync combat state without overwriting TacticalAI position')
req('tacticalAI_.agents()[i].position' in engine and 'damage_.targets()[i].position' in engine,'AI/Damage position invariant missing')

# Bounded action work and no magical-awareness firing.
for token in ('AIActionState','MoveToCover','Peek','Reload','Suppress','Flank','Retreat','Search'):
    req(token in ai,f'009-E action contract missing: {token}')
req('kMaxAgents = 32' in ai or 'kMaxAgents=32' in ai,'AI cap must remain 32')
req('kMaxDecisionsPerStep = 4' in ai or 'kMaxDecisionsPerStep=4' in ai,'AI decision budget must remain bounded to 4 per slice')
req('processed<kMaxDecisionsPerStep' in ai,'Decision loop must enforce its bounded budget')
req('AIPerceptionSource::Squad' in ai and 'squadShareRangeMeters' in ai and 'squadShareFreshSeconds' in ai,'Bounded squad knowledge sharing missing')
req('perceptionSource!=AIPerceptionSource::Vision' in ai,'AI firing must require fresh visual provenance')
req('weapon.previewShot' in ai and 'weapon.canFireNow' in ai,'AI fire authorization must consume WeaponCore/Aim Truth contracts')
req('world.raycastSegment(preview.origin,ballisticProbe)' in ai,'AI fire authorization must verify authoritative ballistic path')
req('fireAuthorized' in ai,'AI fire authorization state missing')

# 009-E must not fake production AI projectile damage before player/team targeting exists.
for forbidden in ('ballistics_.spawn(ai', 'aiBallistics_.spawn', 'playerDamageTarget', 'magicDamagePlayer'):
    req(forbidden not in engine+ai,f'Forbidden premature AI projectile/damage path detected: {forbidden}')

# Regression coverage required before merge.
for token in (
    'Cover truth',
    'Wall blocks player',
    'No firing through cover',
    'Lost LOS',
    'AI reload action',
    'Incapacitated-but-alive',
    'one-way hit-target mirror',
    '32-agent stress',
    'Repeated identical scenarios',
    'METSE Build 009-E Tactical AI Actions + Cover Truth Tests: PASS'
):
    req(token in tests,f'009-E regression coverage missing: {token}')
req('TacticalAICore::kMaxDecisionsPerStep' in tests,'32-agent stress must assert the fixed decision budget')
req('fireAuthorized' in tests and 'AIPerceptionSource::Hearing' in tests and 'AIPerceptionSource::Vision' in tests,'No-magical-awareness/fire provenance tests missing')
req('syncTargetPosition' in tests,'TacticalAI->Damage mirror regression missing')
req('testOnlyDrainAgentMagazine' in tests,'AI reload regression must exercise WeaponCore state')

# PR CI is mandatory so development branches are tested before touching main.
req('pull_request:' in workflow,'iOS workflow must run for pull requests before merge')
req('branches: [ main ]' in workflow,'Pull-request CI must target main')
req('Source integrity' in workflow and 'Architecture guardrails' in workflow and 'C++ mega combat foundation tests' in workflow and
    'Swift syntax fast gate' in workflow and 'Metal compile fast gate' in workflow and 'Xcode platform compile gate' in workflow and
    'Build native unsigned IPA' in workflow,'Full Build 009 gate chain must remain mandatory')
req('python3 Scripts/guardrails_009e.py' in workflow,'009-E guardrail must be part of CI')

if errors:
    print('METSE BUILD 009-E AI ACTION/COVER GUARDRAILS: FAIL')
    for error in errors:
        print(' -',error)
    sys.exit(1)
print('METSE BUILD 009-E AI ACTION/COVER GUARDRAILS: PASS')
