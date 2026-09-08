#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
metse::DamageIntersection traceAt(metse::DamageCore& damage,double y,double radialZ=0.0){
    const auto& target=damage.targets()[0];
    return damage.traceSegment({target.position.x-1.0,y,target.position.z+radialZ},
                               {target.position.x+1.0,y,target.position.z+radialZ});
}
metse::DamageResult applyAt(metse::DamageCore& damage,double y,double energy,std::uint64_t correlation,double radialZ=0.0){
    const auto& target=damage.targets()[0];
    return damage.applySegment({target.position.x-1.0,y,target.position.z+radialZ},
                               {target.position.x+1.0,y,target.position.z+radialZ},energy,correlation);
}
}

int main(){
    using namespace metse;

    // Region classification is deterministic and uses the same target geometry as
    // ballistic tracing. Arms occupy the outer cylinder; legs are vertically distinct.
    DamageCore regions;
    assert(traceAt(regions,1.72).region==HitRegion::Head);
    assert(traceAt(regions,1.53).region==HitRegion::Neck);
    assert(traceAt(regions,1.25).region==HitRegion::Thorax);
    assert(traceAt(regions,0.88).region==HitRegion::Abdomen);
    assert(traceAt(regions,1.25,0.30).region==HitRegion::Arm);
    assert(traceAt(regions,0.50).region==HitRegion::Leg);

    // Torso armor is explicit state: it absorbs bounded energy, degrades, and leaves
    // a readable result rather than hiding the effect inside a damage multiplier.
    DamageCore armor;
    const double torsoBefore=armor.targets()[0].torsoArmorJoules;
    auto torso=applyAt(armor,1.25,1300.0,5001);
    assert(torso.hit&&torso.region==HitRegion::Thorax&&torso.armorZone==ArmorZone::Torso);
    assert(torso.armorHit&&torso.armorAbsorbedJoules>0.0);
    assert(armor.targets()[0].torsoArmorJoules<torsoBefore);
    assert(torso.reactionDirection.x>0.99);
    assert(armor.metrics().armorHits==1);
    assert(armor.validate());

    // The result ledger must retain every result in sequence, not only the latest hit.
    DamageCore ledger;
    auto first=applyAt(ledger,0.50,80.0,5101);
    auto second=applyAt(ledger,0.50,80.0,5102);
    assert(first.hit&&second.hit&&first.sequence+1==second.sequence);
    DamageResult fetchedFirst{},fetchedSecond{};
    assert(ledger.resultBySequence(first.sequence,fetchedFirst));
    assert(ledger.resultBySequence(second.sequence,fetchedSecond));
    assert(fetchedFirst.correlationId==5101&&fetchedSecond.correlationId==5102);

    // Incapacitation is a distinct combat state. An incapacitated target remains a
    // world target but is no longer combat-capable for Tactical AI ownership.
    DamageCore incapacitation;
    DamageResult last{};
    for(std::uint64_t i=0;i<5;++i) last=applyAt(incapacitation,0.50,400.0,5200+i);
    assert(last.hit&&!last.killed&&last.incapacitated);
    assert(incapacitation.targets()[0].combatState==CombatState::Incapacitated);
    assert(incapacitation.targets()[0].alive);
    assert(!DamageCore::combatCapable(incapacitation.targets()[0]));
    assert(incapacitation.totalIncapacitations()==1);
    assert(incapacitation.validate());

    // Bleeding transitions use the correlation of the injury that caused the ongoing
    // state. No per-frame DamageApplied spam is generated; only a state transition is queued.
    DamageCore bleeding;
    for(std::uint64_t i=0;i<4;++i) applyAt(bleeding,0.50,400.0,5300+i);
    assert(bleeding.targets()[0].combatState==CombatState::Wounded);
    const auto sequenceBeforeBleed=bleeding.resultSequence();
    for(int i=0;i<360 && bleeding.targets()[0].combatState!=CombatState::Incapacitated;++i)
        bleeding.fixedStep(1.0/60.0);
    assert(bleeding.targets()[0].combatState==CombatState::Incapacitated);
    assert(bleeding.resultSequence()==sequenceBeforeBleed+1);
    DamageResult bleedTransition{};
    assert(bleeding.resultBySequence(bleeding.resultSequence(),bleedTransition));
    assert(bleedTransition.cause==DamageCause::Bleeding&&bleedTransition.incapacitated);
    assert(bleedTransition.correlationId==5303);
    assert(bleeding.metrics().bleedTransitions==1);
    assert(bleeding.validate());

    // Engine regression: two projectiles may hit in the exact same fixed slice. Both
    // correlated DamageApplied events must survive the atomic publication boundary.
    EngineCore engine;
    const auto targets=engine.damageTargets();
    ShotSolution a{};
    a.origin={targets[0].position.x,1.20,targets[0].position.z-0.80};
    a.direction={0.0,0.0,1.0};
    a.muzzleVelocity=100.0;
    a.massKg=0.004;
    a.correlationId=5401;
    ShotSolution b{};
    b.origin={targets[1].position.x,1.20,targets[1].position.z-0.80};
    b.direction={0.0,0.0,1.0};
    b.muzzleVelocity=100.0;
    b.massKg=0.004;
    b.correlationId=5402;
    const auto eventsBefore=engine.diagnostics().retainedEvents;
    assert(engine.testOnlySpawnProjectile(a));
    assert(engine.testOnlySpawnProjectile(b));
    engine.advance(1.0/60.0);
    const auto diagnostics=engine.diagnostics();
    assert(engine.snapshot().damageHits==2);
    assert(diagnostics.retainedEvents==eventsBefore+2);
    EventRecord newest{},previous{};
    assert(engine.testOnlyNewestIntegrityEvent(0,newest));
    assert(engine.testOnlyNewestIntegrityEvent(1,previous));
    assert(newest.kind==EventKind::DamageApplied&&previous.kind==EventKind::DamageApplied);
    assert(newest.correlationId==5402&&previous.correlationId==5401);
    assert(diagnostics.journalValid&&diagnostics.damageValid&&diagnostics.simulationInvariantRollbacks==0);

    std::cout<<"METSE Build 009 Anatomy + Damage Ledger Tests: PASS\n";
}
