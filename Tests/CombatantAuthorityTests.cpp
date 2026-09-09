#include "../Engine/Core/METSEBallisticsCore.hpp"
#include "../Engine/Core/METSECombatantCore.hpp"
#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace metse;

    // Build 010-A covers AI shots, friendly-fire filtering, and player impacts.

    CombatantCore authority;
    const CombatantIdentity player{CombatantCore::kPlayerId,CombatantCore::kPlayerTeam,
                                   CombatantCore::kPlayerFaction,CombatantRole::Player};
    const CombatantIdentity hostile{1,CombatantCore::kHostileTeam,CombatantCore::kHostileFaction,CombatantRole::AI};
    const CombatantIdentity friendly{2,CombatantCore::kPlayerTeam,CombatantCore::kPlayerFaction,CombatantRole::AI};
    assert(authority.configure(0,player));
    assert(authority.configure(1,hostile));
    assert(authority.configure(2,friendly));
    assert(authority.validate());
    assert(CombatantCore::relation(hostile,player)==TargetRelation::Hostile);
    assert(CombatantCore::relation(player,friendly)==TargetRelation::Friendly);
    assert(CombatantCore::canTarget(DamageSource{hostile,TargetingPolicy::HostileOnly,true},player));
    assert(!CombatantCore::canTarget(DamageSource{player,TargetingPolicy::HostileOnly,true},friendly));
    assert(CombatantCore::canTarget(DamageSource{player,TargetingPolicy::AllowFriendlyFire,true},friendly));

    DamageCore damage;
    const CombatantIdentity configuredPlayer{CombatantCore::kPlayerId,CombatantCore::kPlayerTeam,
                                             CombatantCore::kPlayerFaction,CombatantRole::Player};
    assert(damage.configurePlayerTarget(configuredPlayer));
    assert(damage.setPlayerTargetEnabled(true));
    assert(damage.syncPlayerTargetPosition(configuredPlayer.id,{0.0,0.0,0.0}));
    const DamageSource hostileSource{hostile,TargetingPolicy::HostileOnly,true};
    auto playerHit=damage.traceSegment({0.0,1.70,-1.0},{0.0,1.70,1.0},hostileSource);
    assert(playerHit.hit&&playerHit.playerTarget&&playerHit.targetId==configuredPlayer.id);
    const auto applied=damage.applyIntersection(playerHit,760.0,0x100000001ull,{0.0,0.0,1.0});
    assert(applied.hit&&damage.playerTarget().health<100.0&&damage.validate());

    DamageCore friendlyDamage;
    assert(friendlyDamage.configurePlayerTarget(configuredPlayer));
    assert(friendlyDamage.setPlayerTargetEnabled(true));
    assert(friendlyDamage.syncPlayerTargetPosition(configuredPlayer.id,{0.0,0.0,0.0}));
    const auto blocked=friendlyDamage.traceSegment({0.0,1.70,-1.0},{0.0,1.70,1.0},
                                                    DamageSource{player,TargetingPolicy::HostileOnly,true});
    assert(!blocked.hit&&friendlyDamage.friendlyFireDenials()>0);
    assert(friendlyDamage.playerTarget().health==100.0);

    WorldCollisionCore world;
    DamageCore ballisticDamage;
    assert(ballisticDamage.configurePlayerTarget(configuredPlayer));
    assert(ballisticDamage.setPlayerTargetEnabled(true));
    assert(ballisticDamage.syncPlayerTargetPosition(configuredPlayer.id,{0.0,0.0,0.0}));
    BallisticsCore ballistics;
    ShotSolution hostileShot{};
    hostileShot.origin={0.0,1.70,-1.0};
    hostileShot.direction={0.0,0.0,1.0};
    hostileShot.muzzleVelocity=820.0;
    hostileShot.massKg=0.004;
    hostileShot.correlationId=0x200000001ull;
    hostileShot.sourceCombatantId=hostile.id;
    hostileShot.sourceTeamId=hostile.teamId;
    hostileShot.sourceFactionId=hostile.factionId;
    hostileShot.includePlayerTarget=true;
    assert(ballistics.spawn(hostileShot));
    for(int i=0;i<4&&ballisticDamage.totalHits()==0;++i)
        ballistics.fixedStep(1.0/60.0,world,ballisticDamage);
    assert(ballisticDamage.totalHits()==1);
    assert(ballistics.metrics().targetImpacts==1);
    assert(ballisticDamage.playerTarget().health<100.0);

    EngineCore core;
    assert(core.setActiveCombatants(32));
    assert(core.snapshot().activeCombatants==32);
    assert(core.combatants().count()==32);
    assert(core.damageTargetCount()==31);
    for(int i=0;i<1200;++i) core.advance(1.0/60.0);
    assert(core.diagnostics().aiShotsFired>0);
    assert(core.diagnostics().aiTargetImpacts>0);
    assert(core.diagnostics().combatants.validate());
    assert(core.diagnostics().damageValid);
    assert(core.snapshot().combatantCount==32);

    EngineCore repeat;
    assert(repeat.setActiveCombatants(32));
    for(int i=0;i<1200;++i) repeat.advance(1.0/60.0);
    assert(core.deterministicStateHash()==repeat.deterministicStateHash());

    std::cout<<"METSE Build 010-A Combatant Authority + Player Damage Tests: PASS\n";
    return 0;
}
