#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors: list[str] = []


def text(path: str) -> str:
    p = ROOT / path
    if not p.is_file():
        errors.append(f"missing required file: {path}")
        return ""
    return p.read_text(encoding="utf-8")


def require(haystack: str, needle: str, label: str) -> None:
    if needle not in haystack:
        errors.append(f"missing contract: {label}")


world_h = text("Engine/Core/METSEWorldCollision.hpp")
world_cpp = text("Engine/Core/METSEWorldCollision.cpp")
audio_h = text("Engine/Core/METSEAudioFXCore.hpp")
audio_cpp = text("Engine/Core/METSEAudioFXCore.cpp")
visibility_h = text("Engine/Core/METSEVisibilityCore.hpp")
visibility_cpp = text("Engine/Core/METSEVisibilityCore.cpp")
engine_h = text("Engine/Core/METSEEngineCore.hpp")
engine_cpp = text("Engine/Core/METSEEngineCore.cpp")
tests = text("Tests/AudioFXVisibilityTests.cpp")
runner = text("Scripts/test_engine_core.sh")
workflow = text(".github/workflows/build-ios-unsigned.yml")

# WorldCollision remains the SSOT for traversable surface and acoustic enclosure.
require(world_h, "static constexpr std::size_t kMaxSurfacePatches=6;", "bounded surface semantic patches")
require(world_h, "surfaceMaterialAt", "surface material query owned by WorldCollision")
require(world_h, "hasOverheadCover", "overhead acoustic query owned by WorldCollision")
require(world_cpp, "Areas outside every patch are Soil", "surface fallback rationale")
require(world_cpp, "raycastSegment(position,top)", "indoor/outdoor query derives from authoritative geometry")

# No dynamic/unbounded audio or effect state in the simulation hot path.
require(audio_h, "static constexpr std::size_t kCueCapacity=64;", "audio cue ring hard cap")
require(audio_h, "static constexpr std::size_t kFXCapacity=48;", "FX pool hard cap")
require(audio_h, "static constexpr std::size_t kProjectileCueMemoryCapacity=64;", "near-miss dedupe hard cap")
require(audio_h, "std::array<AudioCue,kCueCapacity>", "fixed audio cue ring")
require(audio_h, "std::array<FXInstance,kFXCapacity>", "fixed FX pool")
require(audio_cpp, "if(!hostileToListener", "near-miss requires explicit hostile projectile provenance")
require(audio_cpp, "memory.crackEmitted", "bullet crack dedupe")
require(audio_cpp, "memory.nearMissEmitted", "near-miss dedupe")
require(audio_cpp, "world.surfaceMaterialAt", "footsteps consume World SSOT material")
require(audio_cpp, "world.hasOverheadCover", "shot acoustic layer consumes World SSOT enclosure")
require(audio_cpp, "deterministicFingerprint", "audio FX determinism fingerprint")

# Visibility is a deterministic bounded presentation budget; it is not AI perception.
require(visibility_h, "static constexpr std::uint32_t kFullBudget=8;", "Full visibility budget")
require(visibility_h, "static constexpr std::uint32_t kReducedBudget=12;", "Reduced visibility budget")
require(visibility_h, "static constexpr std::uint32_t kMinimalBudget=12;", "Minimal visibility budget")
require(visibility_cpp, "Fixed-capacity insertion sort", "bounded deterministic visibility ordering")
require(visibility_cpp, "keyId<previousId", "stable visibility tie break")

# EngineCore owns AudioFX lifecycle and includes it in transaction rollback and hash truth.
require(engine_h, "AudioFXCore audioFX_{};", "EngineCore owns AudioFX")
require(engine_h, "AudioFXCore audioFX{};", "atomic checkpoint contains AudioFX")
require(engine_h, "audioFX_=cp.audioFX;", "atomic command rollback restores AudioFX")
require(engine_cpp, "audioFX_.reset();", "session reset clears AudioFX")
require(engine_cpp, "audioFX_.observeShot(shot.origin,id,world_);", "shot cue emitted inside simulation fire transaction")
require(engine_cpp, "const AudioFXCore audioFXCheckpoint=audioFX_;", "fixed-step rollback checkpoints AudioFX")
require(engine_cpp, "audioFX_=audioFXCheckpoint;", "fixed-step rollback restores AudioFX")
require(engine_cpp, "audioFX_.observeMovement", "footsteps derive from accepted simulation movement")
require(engine_cpp, "state_.audioFX=audioFX_.report();", "snapshot publishes AudioFX report")
require(engine_cpp, "audioFX_.deterministicFingerprint()", "deterministic state hash includes AudioFX")
require(engine_cpp, "diagnostics.audioFXValid=audioFX_.validate();", "diagnostics validates AudioFX")

# Regression suite and strict-gate wiring are mandatory.
for contract in (
    "surfacePatchCount()==WorldCollisionCore::kMaxSurfacePatches",
    "report.footsteps==1",
    "report.outdoorShots==1&&report.indoorShots==1",
    "report.bulletCracks==1&&report.nearMisses==1",
    "audioA.deterministicFingerprint()==audioB.deterministicFingerprint()",
    "budgeted.full==VisibilityCore::kFullBudget",
    "budgeted.reduced==VisibilityCore::kReducedBudget",
    "budgeted.minimal==VisibilityCore::kMinimalBudget",
    "Build 009-G Audio FX + Visibility Tests: PASS",
):
    require(tests, contract, f"009-G regression contract {contract}")

require(runner, "Tests/AudioFXVisibilityTests.cpp", "009-G suite compiled by strict C++ runner")
require(workflow, "python3 Scripts/guardrails_009g.py", "009-G guardrail runs in iOS CI")

if errors:
    print("BUILD 009-G GUARDRAILS: FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("BUILD 009-G GUARDRAILS: PASS")
