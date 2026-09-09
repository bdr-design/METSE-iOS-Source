#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace metse;
namespace {
constexpr CombatantIdentity player{CombatantCore::kPlayerId,1,1,CombatantRole::Player};
constexpr CombatantIdentity enemy{1,2,2,CombatantRole::AI};

ProjectileSegmentObservation segment(double x=6.5) {
    return {{x,1.15,-1.0},{x,1.15,1.0},300.0,17,true,false,player.id,player.teamId,player.factionId};
}

void boundariesAndKnowledge() {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,player));
    assert(authority.configure(1,enemy));
    for(const double offset:{-0.000001,0.0,0.000001}){
        TacticalAICore ai;
        assert(ai.syncAgent(0,enemy.id,{5,0,0},0,true));
        ai.observeProjectileSegment(segment(6.5+offset),authority,world);
        assert((ai.report().suppressedAgents==1)==(offset<=0.0));
        assert(ai.agents()[0].perceptionSource==AIPerceptionSource::None);
        assert(ai.agents()[0].lastKnownPlayerPosition.x==0.0);
        assert(ai.validate());
    }
    TacticalAICore ai;
    assert(ai.syncAgent(0,enemy.id,{5,0,0},0,true));
    auto invalid=segment();
    invalid.traversed=false;
    ai.observeProjectileSegment(invalid,authority,world);
    invalid=segment(); invalid.to=invalid.from;
    ai.observeProjectileSegment(invalid,authority,world);
    invalid=segment(); invalid.from.x=std::numeric_limits<double>::quiet_NaN();
    ai.observeProjectileSegment(invalid,authority,world);
    invalid=segment(); invalid.sourceTeamId=99;
    ai.observeProjectileSegment(invalid,authority,world);
    invalid=segment(); invalid.sourceCombatantId=999;
    ai.observeProjectileSegment(invalid,authority,world);
    invalid=segment(); invalid.correlationId=0;
    ai.observeProjectileSegment(invalid,authority,world);
    assert(ai.report().suppressionObservations==0);
    ai.observeProjectileSegment(segment(),authority,world);
    // No Vision/hearing: pressure cannot reveal the player across the map.
    ai.fixedStep(1.0/60.0,world,{-44,0,44},{},0.0);
    assert(ai.agents()[0].perceptionSource==AIPerceptionSource::None);
    assert(ai.agents()[0].action==AIActionState::Hold);
    assert(!ai.agents()[0].fireAuthorized);
    for(int i=0;i<120;++i) ai.fixedStep(1.0/60.0,world,{-44,0,44},{},0.0);
    assert(ai.agents()[0].suppression01==0.0);
    assert(ai.agents()[0].lastSuppressionCorrelationId==0);
    assert(ai.validate());

    TacticalAICore threshold;
    assert(threshold.syncAgent(0,enemy.id,{5,0,0},0,true));
    threshold.observeProjectileSegment(segment(),authority,world);
    // Exact representable decay probes the inclusive threshold independently of
    // accumulated floating-point rounding in repeated 60 Hz steps.
    threshold.fixedStep(0.5,world,{-44,0,44},{},0.0);
    assert(threshold.agents()[0].suppression01==TacticalAICore::kSuppressionThreshold);
    assert(threshold.report().suppressedAgents==1);
    threshold.fixedStep(1.0/60.0,world,{-44,0,44},{},0.0);
    assert(threshold.report().suppressedAgents==0);

    // Pressure must not grant fire authorization even with fresh Vision.
    assert(ai.syncAgent(0,enemy.id,{5,0,0},0,true));
    ai.observeProjectileSegment(segment(),authority,world);
    ai.fixedStep(1.0/60.0,world,{5,0,10},{},0.0);
    assert(ai.agents()[0].hasLineOfSight);
    std::array<ShotSolution,TacticalAICore::kMaxAgents> shots{};
    assert(ai.fireAuthorizedShots(shots.size(),shots)==0);
    assert(ai.syncAgentCombatState(0,enemy.id,true,false,0.1));
    ai.observeProjectileSegment(segment(),authority,world);
    assert(ai.agents()[0].suppression01==0.0&&ai.validate());
}

void identityAndWalls() {
    WorldCollisionCore world;
    for(const CombatantIdentity source:{CombatantIdentity{2,2,2,CombatantRole::AI},
                                        CombatantIdentity{2,3,2,CombatantRole::AI},enemy}){
        CombatantCore authority;
        assert(authority.configure(0,enemy));
        if(source.id!=enemy.id) assert(authority.configure(1,source));
        TacticalAICore ai;
        assert(ai.syncAgent(0,enemy.id,{5,0,0},0,true));
        auto observed=segment();
        observed.sourceCombatantId=source.id;
        observed.sourceTeamId=source.teamId;
        observed.sourceFactionId=source.factionId;
        ai.observeProjectileSegment(observed,authority,world);
        assert(ai.report().suppressionObservations==0);
    }
    CombatantCore authority;
    assert(authority.configure(0,player));
    assert(authority.configure(1,enemy));
    TacticalAICore ai;
    assert(ai.syncAgent(0,enemy.id,{30.5,0,14},0,true));
    auto observed=segment();
    observed.from={29.8,1.15,13}; observed.to={29.8,1.15,15};
    ai.observeProjectileSegment(observed,authority,world);
    assert(ai.report().suppressionObservations==0); // thin wood still shields exposure
}

struct ObserverContext { TacticalAICore* ai; const CombatantCore* authority; const WorldCollisionCore* world; };
void observe(void* raw,const ProjectileSegmentObservation& observed) noexcept {
    auto& context=*static_cast<ObserverContext*>(raw);
    context.ai->observeProjectileSegment(observed,*context.authority,*context.world);
}

ShotSolution shot(Vec3 origin,std::uint64_t correlation=1) {
    ShotSolution out{};
    out.origin=origin; out.direction={0,0,1}; out.muzzleVelocity=240; out.massKg=0.004;
    out.correlationId=correlation; out.sourceCombatantId=player.id;
    out.sourceTeamId=player.teamId; out.sourceFactionId=player.factionId;
    return out;
}

void terminationAndTrajectoryTruth() {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,player));
    assert(authority.configure(1,enemy));
    assert(authority.configure(2,{2,2,2,CombatantRole::AI}));
    TacticalAICore ai;
    assert(ai.syncAgent(0,1,{8,0,10},0,true));
    assert(ai.syncAgent(1,2,{8,0,18},0,true));
    DamageCore damage,controlDamage;
    assert(damage.configureAgentCount(0));
    assert(controlDamage.configureAgentCount(0));
    BallisticsCore rounds,control;
    assert(rounds.spawn(shot({8,1.15,10})));
    assert(control.spawn(shot({8,1.15,10})));
    ObserverContext context{&ai,&authority,&world};
    rounds.fixedStep(1.0/60.0,world,damage,observe,&context);
    control.fixedStep(1.0/60.0,world,controlDamage);
    assert(rounds.activeCount()==0&&rounds.metrics().terminalWorldImpacts==1);
    assert(rounds.projectiles()[0].position.z==control.projectiles()[0].position.z);
    assert(rounds.metrics().worldImpacts==control.metrics().worldImpacts);
    assert(ai.agents()[0].suppression01>0.0&&ai.agents()[1].suppression01==0.0);
    const auto observations=ai.report().suppressionObservations;
    for(int i=0;i<10;++i) rounds.fixedStep(1.0/60.0,world,damage,observe,&context);
    assert(ai.report().suppressionObservations==observations); // no post-termination exposure
}

void budgetStressAndRollback() {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,player));
    TacticalAICore ai,repeated;
    for(std::size_t i=0;i<31;++i){
        const auto id=static_cast<CombatantId>(i+1);
        assert(authority.configure(i+1,{id,2,2,CombatantRole::AI}));
        assert(ai.syncAgent(i,id,{5,0,0},0,true));
        assert(repeated.syncAgent(i,id,{5,0,0},0,true));
    }
    for(int frame=0;frame<120;++frame){
        const auto before=ai.report().decisionsExecuted;
        ai.fixedStep(1.0/60.0,world,{-44,0,44},{},0.0);
        repeated.fixedStep(1.0/60.0,world,{-44,0,44},{},0.0);
        assert(ai.report().decisionsExecuted-before<=TacticalAICore::kMaxDecisionsPerStep);
        for(int i=0;i<512;++i){
            auto observed=segment(); observed.correlationId=static_cast<std::uint64_t>(i+1);
            ai.observeProjectileSegment(observed,authority,world);
            repeated.observeProjectileSegment(observed,authority,world);
        }
        assert(ai.report().suppressionChecksThisStep==TacticalAICore::kMaxSuppressionChecksPerStep);
        assert(ai.report().suppressionBudgetDrops==repeated.report().suppressionBudgetDrops);
        assert(ai.report().suppressionObservations==repeated.report().suppressionObservations);
        assert(ai.report().suppressedAgents==31&&ai.validate());
        for(std::size_t i=0;i<31;++i){
            assert(ai.agents()[i].lastSuppressionCorrelationId==repeated.agents()[i].lastSuppressionCorrelationId);
            assert(ai.agents()[i].suppression01==repeated.agents()[i].suppression01);
        }
    }
    assert(ai.report().suppressionBudgetDrops>0);

    EngineCore engine,control;
    auto near=shot({-9,1.15,16}); near.muzzleVelocity=120;
    assert(engine.testOnlySpawnProjectile(near));
    assert(control.testOnlySpawnProjectile(near));
    const auto before=engine.deterministicStateHash();
    engine.testOnlyFailNextSimulationSlice();
    engine.advance(1.0/60.0);
    assert(engine.diagnostics().simulationInvariantRollbacks==1);
    assert(engine.deterministicStateHash()==before);
    assert(engine.tacticalAI().report().suppressionObservations==0);
    engine.advance(1.0/60.0);
    control.advance(1.0/60.0);
    assert(engine.tacticalAI().report().suppressionObservations>0);
    assert(engine.tacticalAI().validate());
    assert(engine.deterministicStateHash()==control.deterministicStateHash());
    for(int i=0;i<120;++i){
        engine.advance(1.0/60.0); control.advance(1.0/60.0);
        assert(engine.deterministicStateHash()==control.deterministicStateHash());
    }
}

void saturatedHashSensitivity() {
    EngineCore a,b;
    assert(a.setActiveCombatants(32)); assert(b.setActiveCombatants(32));
    for(std::size_t i=0;i<BallisticsCore::kMaxProjectiles;++i){
        auto first=shot({5,5,0},i+1),second=first;
        if(i+1==BallisticsCore::kMaxProjectiles) second.muzzleVelocity+=1.0;
        assert(a.testOnlySpawnProjectile(first)); assert(b.testOnlySpawnProjectile(second));
    }
    assert(a.deterministicStateHash()!=b.deterministicStateHash());
}

void audioProvenance() {
    for(int scenario=0;scenario<4;++scenario){
        EngineCore engine;
        auto passing=shot({1,1.64,-1});
        if(scenario!=0){
            passing.sourceCombatantId=scenario==3?999:enemy.id;
            passing.sourceTeamId=scenario==2?99:enemy.teamId;
            passing.sourceFactionId=enemy.factionId;
        }
        assert(engine.testOnlySpawnProjectile(passing));
        engine.advance(1.0/60.0);
        assert((engine.audioFX().report().nearMisses>0)==(scenario==1));
    }
}
}

int main() {
    boundariesAndKnowledge(); identityAndWalls(); terminationAndTrajectoryTruth();
    budgetStressAndRollback(); saturatedHashSensitivity(); audioProvenance();
    std::cout<<"METSE Build 010-C Tactical Suppression Tests: PASS\n";
}
