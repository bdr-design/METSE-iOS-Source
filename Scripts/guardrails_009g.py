#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    source = ROOT / path
    if not source.is_file():
        errors.append(f"missing required file: {path}")
        return ""
    return source.read_text(encoding="utf-8")


def require(condition: bool, label: str) -> None:
    if not condition:
        errors.append(f"missing contract: {label}")


world_h = text("Engine/Core/METSEWorldCollision.hpp")
world_cpp = text("Engine/Core/METSEWorldCollision.cpp")
ballistics_h = text("Engine/Core/METSEBallisticsCore.hpp")
ballistics_cpp = text("Engine/Core/METSEBallisticsCore.cpp")
audio_h = text("Engine/Core/METSEAudioFXCore.hpp")
audio_cpp = text("Engine/Core/METSEAudioFXCore.cpp")
visibility_h = text("Engine/Core/METSEVisibilityCore.hpp")
visibility_cpp = text("Engine/Core/METSEVisibilityCore.cpp")
engine_h = text("Engine/Core/METSEEngineCore.hpp")
engine_cpp = text("Engine/Core/METSEEngineCore.cpp")
bridge = text("Engine/Platform/Apple/METSEEngineBridge.mm")
presenter_h = text("Engine/Platform/Apple/METSEAudioPresenter.h")
presenter_mm = text("Engine/Platform/Apple/METSEAudioPresenter.mm")
shader = text("Shaders/METSERenderer.metal")
tests = text("Tests/AudioFXVisibilityTests.cpp")
runner = text("Scripts/test_engine_core.sh")
build = text("Scripts/build_unsigned_ipa.sh")
workflow = text(".github/workflows/build-ios-unsigned.yml")
documentation = text("Docs/BUILD009_G_AUDIO_FX_VISIBILITY_AR.md")

# WorldCollision is the only surface/acoustic geometry owner. The acoustic probe is
# hard bounded and derives every return from the same raycast used by gameplay.
for contract in (
    "static constexpr std::size_t kMaxSurfacePatches=6;",
    "static constexpr std::uint8_t kAcousticProbeRayCount=5;",
    "surfaceMaterialAt",
    "acousticProbeAt",
):
    require(contract in world_h, f"WorldCollision 009-G API {contract}")
require("std::array<Vec3,kAcousticProbeRayCount>" in world_cpp,
        "fixed acoustic ray array")
require("raycastSegment(position,endpoint)" in world_cpp,
        "acoustic probe consumes WorldCollision raycast")
require("outside every patch are Soil" in world_cpp,
        "Soil surface fallback rationale")
for material in ("Steel", "Concrete", "Rock", "Wood", "Brick", "Glass"):
    require(f"WorldMaterial::{material}" in world_cpp,
            f"explicit traversable surface {material}")

duplicated_world_consumers = audio_h + audio_cpp + presenter_h + presenter_mm + bridge
for forbidden in ("surfacePatches_", "WorldSurfacePatch", "AcousticZone", "acousticObstacles_"):
    require(forbidden not in duplicated_world_consumers,
            f"no duplicated world/acoustic list outside WorldCollision ({forbidden})")

# No dynamic/unbounded container or heap operation is permitted in 009-G portable
# hot-path owners. Ballistics observes trajectory through a callback, not a queue.
portable_hot_path = audio_h + audio_cpp + visibility_h + visibility_cpp + ballistics_h + ballistics_cpp
portable_code_only = re.sub(r"//[^\n]*|/\*.*?\*/", "", portable_hot_path, flags=re.DOTALL)
for forbidden in (
    "std::vector", "std::deque", "std::list", "std::queue", "std::map",
    "std::unordered_map", "std::unordered_set", "push_back(", "emplace_back(",
    "malloc(", "calloc(", "realloc(",
):
    require(forbidden not in portable_hot_path,
            f"bounded portable hot path forbids {forbidden}")
require(re.search(r"\bnew\s+", portable_code_only) is None,
        "bounded portable hot path forbids heap new")
require("ProjectileSegmentObserver" in ballistics_h and
        "observer(context" in ballistics_cpp,
        "direct bounded Ballistics segment observer")
require("trajectoryQueue" not in portable_hot_path and "segmentQueue" not in portable_hot_path,
        "no second projectile observation queue")

# AudioFX does not duplicate WeaponCore state. It consumes accepted movement, shot
# intent, World truth, and actual Ballistics segments only.
for forbidden in ("WeaponCore", "WeaponState", "ammoInMagazine", "reserveAmmo",
                  "reloadRemaining", "fireCooldown"):
    require(forbidden not in audio_h + audio_cpp,
            f"audio layer must not own duplicated weapon state ({forbidden})")
for contract in (
    "static constexpr std::size_t kCueCapacity=64;",
    "static constexpr std::size_t kFXCapacity=48;",
    "kProjectileCueMemoryCapacity=BallisticsCore::kMaxProjectiles",
    "std::array<AudioCue,kCueCapacity>",
    "std::array<FXInstance,kFXCapacity>",
    "std::array<ProjectileCueMemory,kProjectileCueMemoryCapacity>",
    "kCrackRadiusMeters=12.0",
    "kNearMissRadiusMeters=2.25",
):
    require(contract in audio_h, f"bounded AudioFX contract {contract}")
require("fxDropped" in audio_h and "Deterministic drop-new policy" in audio_cpp and
        "if(slot==kFXCapacity)" in audio_cpp,
        "FX hard-cap drop-new policy")
require("if(!hostileToListener" in audio_cpp and "memory.terminated" in audio_cpp,
        "hostile provenance and post-termination suppression")
require("acousticProbeAt" in audio_cpp and "surfaceMaterialAt" in audio_cpp,
        "AudioFX consumes World acoustic/surface truth")
require("acceptedCharacterState.grounded" in engine_cpp,
        "footsteps require simulation grounded truth")
require("AudioProjectileObserverContext" in engine_cpp and
        "ballistics_.fixedStep(config_.fixedStepSeconds,world_,damage_," in engine_cpp,
        "Engine wires direct Ballistics segment observation")
require("AudioProjectileObserverContext projectileAudio{&audioFX_,cameraPosition(),false};" in engine_cpp,
        "player projectiles remain explicitly non-hostile before faction contract")
for forbidden in ("ballistics_.spawn(ai", "aiBallistics_.spawn", "playerDamageTarget",
                  "magicDamagePlayer"):
    require(forbidden not in engine_cpp + audio_cpp,
            f"009-G must not create premature AI damage path ({forbidden})")

# AudioFX lifecycle remains inside EngineCore atomic/fixed-slice checkpoints.
for contract in (
    "AudioFXCore audioFX_{};",
    "AudioFXCore audioFX{};",
    "audioFX_=cp.audioFX;",
):
    require(contract in engine_h, f"Engine AudioFX ownership {contract}")
for contract in (
    "audioFX_.reset();",
    "audioFX_.observeShot(shot.origin,id,world_);",
    "const AudioFXCore audioFXCheckpoint=audioFX_;",
    "audioFX_=audioFXCheckpoint;",
    "state_.audioFX=audioFX_.report();",
    "audioFX_.deterministicFingerprint()",
    "diagnostics.audioFXValid=audioFX_.validate();",
):
    require(contract in engine_cpp, f"Engine AudioFX integration {contract}")

# VisibilityCore owns presentation LOS and tier allocation. Metal consumes the tier;
# it cannot perform a private target-occlusion decision or bypass Dormant.
for contract in (
    "static constexpr std::uint32_t kFullBudget=8;",
    "static constexpr std::uint32_t kReducedBudget=12;",
    "static constexpr std::uint32_t kMinimalBudget=12;",
    "bool lineOfSight=false;",
    "const WorldCollisionCore& world",
):
    require(contract in visibility_h, f"Visibility budget/LOS contract {contract}")
require("world.raycastSegment(camera,presentationCenter)" in visibility_cpp,
        "VisibilityCore consumes WorldCollision LOS")
require("Fixed-capacity insertion sort" in visibility_cpp and "keyId<previousId" in visibility_cpp,
        "deterministic bounded visibility ordering")
require("visibilityEntities = _core.visibilityCore().entities();" in bridge and
        "uniforms.targetMeta" in bridge,
        "Bridge consumes VisibilityCore entity tiers")
require("targetMeta[kMaxTargets]" in shader and "float tier=tm.x" in shader,
        "Metal consumes visibility tier")
require("targetOccluded(" not in shader,
        "renderer-only target visibility is forbidden")

# Apple bridge/presenter are consumers only. The voice pool and Metal FX budget are
# fixed; muzzle flash derives from FXInstance instead of a duplicate shot counter.
for forbidden in ("std::queue", "std::deque", "dispatch_queue", "NSMutableArray",
                  "applyIntersection", "executeAtomic", "ballistics_.spawn",
                  "tacticalAI_.", "weapon_", "damage_"):
    require(forbidden not in presenter_h + presenter_mm + bridge,
            f"Bridge/presenter must not own gameplay or an unbounded queue ({forbidden})")
require("constexpr std::size_t kAudioVoiceCapacity=12;" in presenter_mm and
        "std::array<METSEAudioVoice,kAudioVoiceCapacity>" in presenter_mm and
        "compare_exchange_strong" in presenter_mm,
        "fixed non-blocking native audio voice pool")
require("droppedVoices.fetch_add" in presenter_mm,
        "native audio saturation drop accounting")
require("kRenderTargetCap = metse::VisibilityCore::kMaxEntities" in bridge,
        "Bridge target cap derives from VisibilityCore")
require("static_assert(kRenderFXCap <= metse::AudioFXCore::kFXCapacity" in bridge and
        "constant uint kMaxFX=16;" in shader,
        "Metal FX presentation budget stays below simulation cap")
require("effect.kind==metse::FXKind::MuzzleFlash" in bridge and
        "state.shotsFired != _lastRenderedShot" not in bridge and "_muzzleFlash" not in bridge,
        "muzzle presentation consumes FX pool without duplicate shot state")

# Mandatory regressions, documentation, build evidence, and CI ordering.
for contract in (
    "verifySurfaceIdentity",
    "CharacterGait::Sprint,false",
    "kAcousticProbeRayCount",
    "kCrackRadiusMeters+1e-6",
    "kNearMissRadiusMeters+1e-6",
    "terminatedAfterSegment",
    "observer.observations==1",
    "report.fxDropped==12",
    "runThirtyTwoCombatantAudioStress",
    "boundaries.entities()[0].tier==VisibilityTier::Reduced",
    "occlusion.report().occluded==1",
    "budgeted.full==VisibilityCore::kFullBudget",
    "stressA.deterministicFingerprint()==stressB.deterministicFingerprint()",
    "engineA.deterministicStateHash()==engineB.deterministicStateHash()",
    "METSE Build 009-G Audio FX + Visibility Tests: PASS",
):
    require(contract in tests, f"009-G regression {contract}")
require("Tests/AudioFXVisibilityTests.cpp" in runner,
        "009-G suite compiled by strict C++ runner")
require("METSEAudioFXCore" in build and "METSEAudioPresenter" in build,
        "unsigned IPA build proves AudioFX and native presenter objects")
require("Player/Team/Faction" in documentation and "drop-new" in documentation and
        "Observatory V4" in documentation,
        "009-G ownership/out-of-scope documentation")
guardrail_step=workflow.find("python3 Scripts/guardrails_009g.py")
strict_step=workflow.find("bash Scripts/test_engine_core.sh")
require(guardrail_step>=0 and strict_step>=0 and guardrail_step<strict_step,
        "009-G guardrail runs before strict C++ in iOS CI")

if errors:
    print("BUILD 009-G GUARDRAILS: FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("BUILD 009-G GUARDRAILS: PASS")
