#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace metse;
namespace {
DamageResult event(std::uint64_t sequence,CombatantId target=1) {
    DamageResult result{};
    result.sequence=sequence; result.targetId=target; result.correlationId=sequence+100;
    result.hit=true; result.damage=5.0; result.remainingHealth=95.0;
    result.newState=CombatState::Wounded;
    result.stateChanged=true;
    return result;
}

void injuryAndSequence() {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,{1,2,2,CombatantRole::AI}));
    TacticalAICore ai;
    assert(ai.syncAgent(0,1,{5,0,0},0,true,true,0.95));
    const auto first=event(1);
    assert(ai.observeDamageResult(first,authority,world));
    assert(ai.agents()[0].health01==0.95); // DamageCore's mirror was not modified
    assert(ai.report().injuryReactions==1&&ai.report().recoveringAgents==1);
    assert(ai.agents()[0].perceptionSource==AIPerceptionSource::None);
    assert(!ai.observeDamageResult(first,authority,world)); // duplicate
    assert(!ai.observeDamageResult(event(3),authority,world)); // gap
    auto bad=event(2); bad.damage=std::numeric_limits<double>::quiet_NaN();
    assert(!ai.observeDamageResult(bad,authority,world));
    bad=event(2); bad.targetId=999;
    assert(!ai.observeDamageResult(bad,authority,world));
    bad=event(2); bad.remainingHealth=std::numeric_limits<double>::quiet_NaN();
    assert(!ai.observeDamageResult(bad,authority,world));
    bad=event(2); bad.killed=true;
    assert(!ai.observeDamageResult(bad,authority,world));
    bad=event(2); bad.cause=static_cast<DamageCause>(255);
    assert(!ai.observeDamageResult(bad,authority,world));
    assert(ai.report().lastObservedDamageSequence==1&&ai.report().injuryReactions==1);
    std::array<ShotSolution,TacticalAICore::kMaxAgents> shots{};
    assert(ai.fireAuthorizedShots(shots.size(),shots)==0);
    for(int i=0;i<22;++i) ai.fixedStep(1.0/60.0,world,{-44,0,44},{},0);
    assert(ai.report().recoveringAgents==0&&ai.agents()[0].lastInjuryCorrelationId==0);
    auto armorOnly=event(2); armorOnly.damage=0;
    assert(ai.observeDamageResult(armorOnly,authority,world));
    auto bleeding=event(3); bleeding.cause=DamageCause::Bleeding;
    assert(ai.observeDamageResult(bleeding,authority,world));
    assert(ai.report().injuryReactions==1); // no continuous bleed-triggered fire lock
    assert(ai.syncAgentCombatState(0,1,true,false,0.1));
    assert(ai.observeDamageResult(event(4),authority,world));
    assert(ai.report().recoveringAgents==0&&ai.validate());

    // A known identity without an AI slot must fail without consuming its sequence.
    assert(authority.configure(1,{2,2,2,CombatantRole::AI}));
    assert(!ai.observeDamageResult(event(5,2),authority,world));
    assert(ai.report().lastObservedDamageSequence==4);
}

std::uint32_t witnessScenario(Vec3 casualty,Vec3 witness,double yaw,TeamId team=2) {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,{1,2,2,CombatantRole::AI},false,false,false));
    assert(authority.configure(1,{2,team,team,CombatantRole::AI}));
    TacticalAICore ai;
    assert(ai.syncAgent(0,1,casualty,0,false,false,0));
    assert(ai.syncAgent(1,2,witness,yaw,true));
    auto loss=event(1); loss.killed=true; loss.newState=CombatState::Dead;
    assert(ai.observeDamageResult(loss,authority,world));
    assert(ai.agents()[1].perceptionSource==AIPerceptionSource::None);
    assert(ai.agents()[1].lastKnownPlayerPosition.x==0&&ai.agents()[1].lastKnownPlayerPosition.z==0);
    assert(ai.agents()[1].health01==1.0&&ai.validate());
    return ai.report().concernedAgents;
}

void witnessBoundariesAndExpiry() {
    assert(witnessScenario({5,0,18},{5,0,0},0)==1);
    assert(witnessScenario({5,0,18.000001},{5,0,0},0)==0);
    assert(witnessScenario({5,0,17.999999},{5,0,0},0)==1);
    assert(witnessScenario({5,0,10},{5,0,0},3.141592653589793)==0);
    assert(witnessScenario({5,0,10},{5,0,0},0,3)==0);
    assert(witnessScenario({8,0,18},{8,0,10},0)==0); // concrete LOS blocker
    TacticalAIConfig config;
    const double edge=config.horizontalFovRadians*0.5;
    assert(witnessScenario({5,0,10},{5,0,0},edge)==1);
    assert(witnessScenario({5,0,10},{5,0,0},edge+0.000001)==0);

    WorldCollisionCore world;
    CombatantCore authority;
    TacticalAICore ai;
    for(std::size_t i=0;i<3;++i){
        const auto id=static_cast<CombatantId>(i+1);
        assert(authority.configure(i,{id,2,2,CombatantRole::AI},true,i!=0,true));
        assert(ai.syncAgent(i,id,{5,0,10.0-static_cast<double>(i)*10.0},0,true,i!=0,i==0?0.1:1.0));
    }
    auto loss=event(1); loss.incapacitated=true; loss.newState=CombatState::Incapacitated;
    assert(ai.observeDamageResult(loss,authority,world));
    assert(ai.agents()[1].teamLossConcernRemaining==TacticalAICore::kTeamLossConcernSeconds);
    assert(ai.agents()[2].teamLossConcernRemaining==0); // no relay from nearby witness
    for(int i=0;i<121;++i) ai.fixedStep(1.0/60.0,world,{-44,0,44},{},0);
    assert(ai.report().concernedAgents==0);
    assert(ai.agents()[1].lastWitnessedLossId==0&&ai.agents()[1].lastWitnessedLossCorrelationId==0);
    assert(ai.validate());
}

void boundedLossStorm() {
    WorldCollisionCore world;
    CombatantCore authority;
    assert(authority.configure(0,{CombatantCore::kPlayerId,1,1,CombatantRole::Player}));
    TacticalAICore a,b;
    for(std::size_t i=0;i<31;++i){
        const auto id=static_cast<CombatantId>(i+1);
        const bool alive=i>=5;
        assert(authority.configure(i+1,{id,2,2,CombatantRole::AI},alive,alive,alive));
        const Vec3 position{5,0,alive?0.0:10.0+static_cast<double>(i)*0.1};
        assert(a.syncAgent(i,id,position,0,alive));
        assert(b.syncAgent(i,id,position,0,alive));
    }
    for(std::uint64_t sequence=1;sequence<=5;++sequence){
        auto loss=event(sequence,static_cast<CombatantId>(sequence));
        loss.killed=true; loss.newState=CombatState::Dead;
        assert(a.observeDamageResult(loss,authority,world));
        assert(b.observeDamageResult(loss,authority,world));
    }
    assert(a.report().lossChecksThisStep==TacticalAICore::kMaxLossChecksPerStep);
    assert(a.report().lossBudgetDrops==1&&a.report().witnessedLosses==104);
    assert(a.report().lastObservedDamageSequence==5);
    assert(a.report().concernedAgents==26&&a.validate());
    for(std::size_t i=0;i<31;++i)
        assert(a.agents()[i].lastWitnessedLossCorrelationId==b.agents()[i].lastWitnessedLossCorrelationId);
    const auto decisions=a.report().decisionsExecuted;
    a.fixedStep(1.0/60.0,world,{-44,0,44},{},0);
    assert(a.report().decisionsExecuted-decisions<=TacticalAICore::kMaxDecisionsPerStep);
    assert(a.report().lossChecksThisStep==0);
}

void realDamageRollbackAndReplay() {
    EngineCore engine,control;
    ShotSolution shot{};
    shot.origin={-10,1.15,17}; shot.direction={0,0,1}; shot.muzzleVelocity=120;
    shot.massKg=0.004; shot.correlationId=900;
    shot.sourceCombatantId=CombatantCore::kPlayerId; shot.sourceTeamId=1; shot.sourceFactionId=1;
    assert(engine.testOnlySpawnProjectile(shot)&&control.testOnlySpawnProjectile(shot));
    const auto before=engine.deterministicStateHash();
    engine.testOnlyFailNextSimulationSlice();
    engine.advance(1.0/60.0);
    assert(engine.deterministicStateHash()==before);
    assert(engine.tacticalAI().report().lastObservedDamageSequence==0);
    assert(engine.tacticalAI().report().injuryReactions==0);
    engine.advance(1.0/60.0); control.advance(1.0/60.0);
    assert(engine.tacticalAI().report().injuryReactions==1);
    assert(engine.damageTargets()[0].health<100.0);
    assert(engine.tacticalAI().agents()[0].lastInjuryCorrelationId==900);
    for(int i=0;i<240;++i){
        assert(engine.deterministicStateHash()==control.deterministicStateHash());
        engine.advance(1.0/60.0); control.advance(1.0/60.0);
    }
}
}

int main() {
    injuryAndSequence(); witnessBoundariesAndExpiry(); boundedLossStorm(); realDamageRollbackAndReplay();
    std::cout<<"METSE Build 010-D Injury / Local Team Reaction Tests: PASS\n";
}
