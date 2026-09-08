# METSE Build 009-E — AI Combat Actions & Cover

## Status
Development branch only until Source Integrity, architecture guardrails, strict C++ tests, Swift, Metal, Xcode platform compile, and unsigned IPA all pass on the same commit.

## Ownership
- `TacticalAICore` owns Tactical AI locomotion, facing, perception memory, action state and AI WeaponCore state.
- `DamageCore` does not own AI movement. It receives a one-way position mirror from `TacticalAICore` before ballistic target traces.
- `EngineCore` remains the simulation owner and coordinates the fixed 60 Hz slice.
- UI/Bridge remain presentation/input adapters and do not make tactical decisions.

## Cover Truth
- Cover candidates are derived from authoritative `WorldCollisionCore` obstacle geometry.
- The pool is bounded to `kMaxObstacles * 4` candidates.
- Overhead-only geometry is rejected as standing cover.
- Cover selection, locomotion collision, peek LOS, muzzle obstruction, and ballistic path authorization consume the same world collision truth.
- No separate Content-authored cover SSOT is introduced.

## Actions
Bounded action state:
- Hold
- MoveToCover
- Peek
- Reload
- Suppress
- Flank
- Retreat
- Search

Decision work is bounded to four expensive tactical decisions per fixed simulation slice. Perception remains bounded to the 32-agent cap.

## Knowledge and Firing Boundary
- Vision, hearing, memory and short-range/fresh squad sharing are the only tactical knowledge sources.
- Hearing and squad sharing use deterministic localization uncertainty.
- Last-known position may drive search/movement but never grants firing permission.
- `fireAuthorized` requires fresh `Vision`, valid action state, `WeaponCore::previewShot`, `WeaponCore::canFireNow`, muzzle clearance and an unobstructed authoritative ballistic probe.

Build 009-E intentionally does **not** create production AI projectile damage against the player yet. The current engine has no complete player `DamageTarget`, faction/team identity, friendly-fire filtering or combatant-target contract. Those must be defined before wiring `AI WeaponCore::fire -> BallisticsCore::spawn -> player DamageTarget`.

## Mandatory Regressions
`Tests/TacticalAIActionCoverTests.cpp` covers:
- world-derived valid cover and rejection of overhead-only cover,
- wall blocks visual awareness,
- no firing through cover,
- lost LOS preserves previous last-known position without magical hidden-coordinate updates,
- AI reload through the existing WeaponCore contract,
- incapacitated-but-alive agents execute no combat action,
- one-way TacticalAI-to-Damage position mirror,
- 32-agent decision budget,
- deterministic repeated scenario including per-agent actions, knowledge, locomotion and AI weapon state.

## Release Rule
Do not change `VERSION=0.3.0` or `BUILD=8` during Build 009 development. Build/Version increments happen only at the final Build 009 release seal.
