#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSETacticalAICore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {
using metse::Vec3;

double distanceXZ(Vec3 a,Vec3 b){
    return std::hypot(a.x-b.x,a.z-b.z);
}

double face(Vec3 from,Vec3 to){
    return std::atan2(to.x-from.x,to.z-from.z);
}

bool sameVec(Vec3 a,Vec3 b){
    return a.x==b.x&&a.y==b.y&&a.z==b.z;
}

void assertAgentEqual(const metse::TacticalAgentState& a,const metse::TacticalAgentState& b){
    assert(a.id==b.id);
    assert(sameVec(a.position,b.position));
    assert(sameVec(a.lastKnownPlayerPosition,b.lastKnownPlayerPosition));
    assert(sameVec(a.actionTarget,b.actionTarget));
    assert(sameVec(a.coverPosition,b.coverPosition));
    assert(sameVec(a.peekPosition,b.peekPosition));
    assert(a.facingYaw==b.facingYaw);
    assert(a.memoryAgeSeconds==b.memoryAgeSeconds);
    assert(a.decisionAgeSeconds==b.decisionAgeSeconds);
    assert(a.actionAgeSeconds==b.actionAgeSeconds);
    assert(a.confidence==b.confidence);
    assert(a.threat==b.threat);
    assert(a.health01==b.health01);
    assert(a.actionSequence==b.actionSequence);
    assert(a.squadSourceAgentId==b.squadSourceAgentId);
    assert(a.coverCandidateIndex==b.coverCandidateIndex);
    assert(a.alert==b.alert);
    assert(a.perceptionSource==b.perceptionSource);
    assert(a.action==b.action);
    assert(a.alive==b.alive);
    assert(a.combatCapable==b.combatCapable);
    assert(a.hasLineOfSight==b.hasLineOfSight);
    assert(a.heardPlayer==b.heardPlayer);
    assert(a.hasCover==b.hasCover);
    assert(a.fireAuthorized==b.fireAuthorized);
}
}

int main(){
    using namespace metse;
    constexpr double kPi=3.14159265358979323846;

    WorldCollisionCore world;
    assert(world.validate());
    assert(world.coverCandidateCount()>0);
    assert(world.coverCandidateCount()<=WorldCollisionCore::kMaxCoverCandidates);

    // Cover truth: every published candidate is derived from a real obstacle and the
    // owning obstacle blocks a ray cast through its inward side. Overhead-only geometry
    // must never appear as standing tactical cover.
    bool foundObstacleZeroCover=false;
    bool foundOverheadCover=false;
    for(std::size_t i=0;i<world.coverCandidateCount();++i){
        const auto& candidate=world.coverCandidates()[i];
        assert(candidate.valid);
        assert(candidate.obstacleIndex<world.obstacleCount());
        if(candidate.obstacleIndex==0) foundObstacleZeroCover=true;
        if(candidate.obstacleIndex==5) foundOverheadCover=true;
        const Vec3 inwardThreat{candidate.position.x-candidate.outwardNormal.x*4.0,
                               candidate.position.y,
                               candidate.position.z-candidate.outwardNormal.z*4.0};
        const auto coverHit=world.raycastSegment({candidate.position.x,1.34,candidate.position.z},
                                                 {inwardThreat.x,1.15,inwardThreat.z});
        assert(coverHit.hit);
        assert(coverHit.obstacleIndex==candidate.obstacleIndex);
    }
    assert(foundObstacleZeroCover);
    assert(!foundOverheadCover);

    // Wall blocks player: obstacle 0 lies directly between this agent and player.
    TacticalAICore blocked;
    const Vec3 blockedAgent{8.0,0.0,10.0};
    const Vec3 hiddenPlayer{8.0,0.0,19.0};
    assert(blocked.syncAgent(0,101,blockedAgent,0.0,true,true,1.0));
    blocked.fixedStep(0.25,world,hiddenPlayer,{0,0,0},0.0);
    assert(!blocked.agents()[0].hasLineOfSight);
    assert(blocked.agents()[0].perceptionSource==AIPerceptionSource::None);
    assert(!blocked.agents()[0].fireAuthorized);
    assert(blocked.validate());

    // No firing through cover: hearing may produce an inexact last-known estimate, but
    // it cannot become fire permission while the authoritative world ray remains blocked.
    TacticalAICore heardBehindCover;
    const Vec3 coverAgent{8.0,0.0,17.58};
    const Vec3 playerOtherSide{8.0,0.0,8.0};
    assert(heardBehindCover.syncAgent(0,102,coverAgent,kPi,true,true,1.0));
    heardBehindCover.fixedStep(0.25,world,playerOtherSide,{0,0,0},1.0);
    const auto& heard=heardBehindCover.agents()[0];
    assert(!heard.hasLineOfSight);
    assert(heard.perceptionSource==AIPerceptionSource::Hearing);
    assert(distanceXZ(heard.lastKnownPlayerPosition,playerOtherSide)>0.1);
    assert(!heard.fireAuthorized);
    assert(heardBehindCover.validate());

    // Lost LOS must preserve only the previous visual last-known position. Moving the
    // player behind a wall must not magically update hidden coordinates or authorize fire.
    TacticalAICore memory;
    const Vec3 memoryAgent{8.0,0.0,10.0};
    const Vec3 visiblePlayer{8.0,0.0,5.0};
    assert(memory.syncAgent(0,103,memoryAgent,kPi,true,true,1.0));
    memory.fixedStep(0.25,world,visiblePlayer,{0,0,0},0.0);
    assert(memory.agents()[0].hasLineOfSight);
    assert(memory.agents()[0].perceptionSource==AIPerceptionSource::Vision);
    const Vec3 visualLastKnown=memory.agents()[0].lastKnownPlayerPosition;
    assert(sameVec(visualLastKnown,visiblePlayer));
    memory.fixedStep(0.25,world,hiddenPlayer,{0,0,0},0.0);
    const auto& remembered=memory.agents()[0];
    assert(!remembered.hasLineOfSight);
    assert(remembered.perceptionSource==AIPerceptionSource::Vision);
    assert(sameVec(remembered.lastKnownPlayerPosition,visualLastKnown));
    assert(!sameVec(remembered.lastKnownPlayerPosition,hiddenPlayer));
    assert(!remembered.fireAuthorized);
    assert(memory.validate());

    // AI reload action uses the same WeaponCore contract. Emptying the bounded magazine
    // must transition to Reload rather than synthesizing ammo or continuing fire.
    TacticalAICore reloadAI;
    const Vec3 reloadAgent{-10.0,0.0,18.0};
    const Vec3 reloadPlayer{0.0,0.0,0.0};
    assert(reloadAI.syncAgent(0,104,reloadAgent,face(reloadAgent,reloadPlayer),true,true,1.0));
    assert(reloadAI.testOnlyDrainAgentMagazine(0));
    const auto* drained=reloadAI.agentWeaponState(0);
    assert(drained!=nullptr&&drained->ammoInMagazine==0&&drained->reserveAmmo>0&&!drained->reloading);
    reloadAI.fixedStep(0.25,world,reloadPlayer,{0,0,0},0.0);
    const auto* reloading=reloadAI.agentWeaponState(0);
    assert(reloading!=nullptr&&reloading->reloading);
    assert(reloadAI.agents()[0].action==AIActionState::Reload);
    assert(!reloadAI.agents()[0].fireAuthorized);
    assert(reloadAI.validate());

    // Incapacitated-but-alive is explicitly not combat capable and must execute no
    // perception/action/fire work.
    TacticalAICore incapacitated;
    assert(incapacitated.syncAgent(0,105,{-10,0,18},face({-10,0,18},{0,0,0}),true,false,0.20));
    incapacitated.fixedStep(0.5,world,{0,0,0},{0,0,0},1.0);
    const auto& down=incapacitated.agents()[0];
    assert(down.alive);
    assert(!down.combatCapable);
    assert(down.action==AIActionState::Hold);
    assert(down.perceptionSource==AIPerceptionSource::None);
    assert(!down.hasLineOfSight&&!down.heardPlayer&&!down.fireAuthorized);
    assert(incapacitated.report().activeAgents==0);
    assert(incapacitated.validate());

    // Tactical locomotion is authoritative and DamageCore is a one-way hit-target mirror.
    DamageCore mirroredDamage;
    const auto targetId=mirroredDamage.targets()[0].id;
    const Vec3 movedTarget{-6.0,0.0,14.0};
    assert(mirroredDamage.syncTargetPosition(0,targetId,movedTarget));
    assert(sameVec(mirroredDamage.targets()[0].position,movedTarget));
    const auto mirroredHit=mirroredDamage.traceSegment({-6.0,1.2,10.0},{-6.0,1.2,18.0});
    assert(mirroredHit.hit&&mirroredHit.targetId==targetId);
    assert(mirroredDamage.validate());

    // 32-agent stress: all agents can become decision-eligible at once, but fixed-step
    // expensive decision work remains strictly bounded to four agents per slice.
    TacticalAICore stress;
    for(std::size_t i=0;i<TacticalAICore::kMaxAgents;++i){
        const double x=-30.0+static_cast<double>(i%8)*2.0;
        const double z=-30.0+static_cast<double>(i/8)*2.0;
        const Vec3 p{x,0.0,z};
        assert(stress.syncAgent(i,1000u+static_cast<std::uint32_t>(i),p,face(p,{0,0,0}),true,true,1.0));
    }
    stress.fixedStep(0.25,world,{0,0,0},{0,0,0},0.0);
    assert(stress.report().activeAgents==32);
    assert(stress.report().decisionsExecuted==TacticalAICore::kMaxDecisionsPerStep);
    assert(stress.validate());

    // Repeated identical scenarios must produce identical per-agent action, knowledge,
    // locomotion and WeaponCore state, not merely the same aggregate report.
    TacticalAICore deterministicA;
    TacticalAICore deterministicB;
    for(std::size_t i=0;i<TacticalAICore::kMaxAgents;++i){
        const double x=-32.0+static_cast<double>(i%8)*2.2;
        const double z=-28.0+static_cast<double>(i/8)*2.4;
        const Vec3 p{x,0.0,z};
        const double yaw=face(p,{0,0,0});
        assert(deterministicA.syncAgent(i,2000u+static_cast<std::uint32_t>(i),p,yaw,true,true,1.0));
        assert(deterministicB.syncAgent(i,2000u+static_cast<std::uint32_t>(i),p,yaw,true,true,1.0));
    }
    for(int tick=0;tick<240;++tick){
        const double phase=static_cast<double>(tick%60)/60.0;
        const Vec3 player{2.0*phase,0.0,-1.5*phase};
        const Vec3 velocity{2.0/60.0,0.0,-1.5/60.0};
        const double noise=(tick%45)<9?0.65:0.0;
        deterministicA.fixedStep(1.0/60.0,world,player,velocity,noise);
        deterministicB.fixedStep(1.0/60.0,world,player,velocity,noise);
    }
    assert(deterministicA.report().decisionsExecuted==deterministicB.report().decisionsExecuted);
    for(std::size_t i=0;i<TacticalAICore::kMaxAgents;++i){
        assertAgentEqual(deterministicA.agents()[i],deterministicB.agents()[i]);
        const auto* weaponA=deterministicA.agentWeaponState(i);
        const auto* weaponB=deterministicB.agentWeaponState(i);
        assert(weaponA!=nullptr&&weaponB!=nullptr);
        assert(weaponA->ammoInMagazine==weaponB->ammoInMagazine);
        assert(weaponA->reserveAmmo==weaponB->reserveAmmo);
        assert(weaponA->shotSequence==weaponB->shotSequence);
        assert(weaponA->fireCooldown==weaponB->fireCooldown);
        assert(weaponA->reloadRemaining==weaponB->reloadRemaining);
        assert(weaponA->sprintRecoveryRemaining==weaponB->sprintRecoveryRemaining);
        assert(weaponA->adsAlpha==weaponB->adsAlpha);
        assert(weaponA->recoilPitch==weaponB->recoilPitch);
        assert(weaponA->recoilYaw==weaponB->recoilYaw);
        assert(weaponA->swayX==weaponB->swayX);
        assert(weaponA->swayY==weaponB->swayY);
        assert(weaponA->reloading==weaponB->reloading);
        assert(weaponA->obstructed==weaponB->obstructed);
        assert(weaponA->reloadKind==weaponB->reloadKind);
    }
    assert(deterministicA.validate()&&deterministicB.validate());

    std::cout<<"METSE Build 009-E Tactical AI Actions + Cover Truth Tests: PASS\n";
    return 0;
}
