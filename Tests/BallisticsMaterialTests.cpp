#include "../Engine/Core/METSEBallisticsCore.hpp"
#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSEMaterialCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
double length(metse::Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
metse::Vec3 normalize(metse::Vec3 v){const double l=length(v);return {v.x/l,v.y/l,v.z/l};}
}

int main(){using namespace metse;
    WorldCollisionCore world;
    assert(world.validate());
    assert(world.obstacleCount()==6);

    // The ray contract exposes entry, exit, face normal, and physical thickness.
    const auto woodHit=world.raycastSegment({13,1,-2},{15,1,-2});
    assert(woodHit.hit&&woodHit.material==WorldMaterial::Wood);
    assert(std::abs(woodHit.thicknessMeters-0.18)<0.002);
    assert(woodHit.exitT>woodHit.t);
    assert(woodHit.normal.x<-0.99&&std::abs(woodHit.normal.y)<1e-9&&std::abs(woodHit.normal.z)<1e-9);
    const auto groundHit=world.raycastSegment({0,1,0},{0,-1,0});
    assert(groundHit.hit&&groundHit.material==WorldMaterial::Soil&&groundHit.normal.y>0.99);

    // All seven 009-C materials are valid even though the stable foundation map
    // only instantiates the first three until the battlefield phase.
    for(WorldMaterial material:{WorldMaterial::Concrete,WorldMaterial::Steel,WorldMaterial::Wood,WorldMaterial::Brick,WorldMaterial::Glass,WorldMaterial::Soil,WorldMaterial::Rock})
        assert(MaterialCore::validateProfile(MaterialCore::ballistic(material)));
    assert(MaterialCore::ballistic(WorldMaterial::Glass).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Brick).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Wood).penetrable);
    assert(!MaterialCore::ballistic(WorldMaterial::Steel).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Steel).ricochetEligible);
    assert(!MaterialCore::ballistic(WorldMaterial::Soil).ricochetEligible);

    // Thickness-aware penetration: this rifle can cross the thin timber partition,
    // retaining bounded velocity and continuing inside the same fixed slice.
    DamageCore woodDamage;
    BallisticsCore woodBallistics;
    ShotSolution woodShot{};
    woodShot.origin={13,1,-2};
    woodShot.direction={1,0,0};
    woodShot.muzzleVelocity=820;
    woodShot.massKg=.004;
    woodShot.correlationId=7001;
    assert(woodBallistics.spawn(woodShot));
    woodBallistics.fixedStep(1.0/60.0,world,woodDamage);
    assert(woodBallistics.metrics().worldImpacts==1);
    assert(woodBallistics.metrics().penetrations==1);
    assert(woodBallistics.metrics().terminalWorldImpacts==0);
    assert(woodBallistics.activeCount()==1);
    assert(woodBallistics.projectiles()[0].penetrations==1);
    assert(woodBallistics.projectiles()[0].position.x>14.18);
    assert(woodBallistics.validate());

    // Glancing steel impact must ricochet at most once and reverse the normal axis.
    DamageCore steelDamage;
    BallisticsCore steelBallistics;
    ShotSolution steelShot{};
    steelShot.origin={-16,1,9};
    steelShot.direction=normalize({0.2,0,1.0});
    steelShot.muzzleVelocity=820;
    steelShot.massKg=.004;
    steelShot.correlationId=7002;
    assert(steelBallistics.spawn(steelShot));
    steelBallistics.fixedStep(1.0/60.0,world,steelDamage);
    assert(steelBallistics.metrics().worldImpacts>=1);
    assert(steelBallistics.metrics().ricochets==1);
    assert(steelBallistics.metrics().terminalWorldImpacts==0);
    assert(steelBallistics.activeCount()==1);
    const auto& steelProjectile=steelBallistics.projectiles()[0];
    assert(steelProjectile.ricochets==1);
    assert(steelProjectile.velocity.x<0.0);
    assert(steelBallistics.validate());

    // Head-on concrete remains terminal; glancing logic cannot turn a direct strike
    // into a ricochet simply because the surface supports ricochet in principle.
    DamageCore concreteDamage;
    BallisticsCore concreteBallistics;
    ShotSolution concreteShot{};
    concreteShot.origin={5,1,14};
    concreteShot.direction={1,0,0};
    concreteShot.muzzleVelocity=820;
    concreteShot.massKg=.004;
    concreteShot.correlationId=7003;
    assert(concreteBallistics.spawn(concreteShot));
    concreteBallistics.fixedStep(1.0/60.0,world,concreteDamage);
    assert(concreteBallistics.metrics().worldImpacts==1);
    assert(concreteBallistics.metrics().penetrations==0);
    assert(concreteBallistics.metrics().ricochets==0);
    assert(concreteBallistics.metrics().terminalWorldImpacts==1);
    assert(concreteBallistics.activeCount()==0);
    assert(concreteBallistics.validate());

    std::cout<<"METSE Build 009 Ballistics + Material Contact Truth Tests: PASS\n";
}
