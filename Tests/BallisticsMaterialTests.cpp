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
    assert(world.obstacleCount()==8);

    const auto glassHit=world.raycastSegment({-35,1,17},{-30,1,17});
    assert(glassHit.hit&&glassHit.material==WorldMaterial::Glass);
    assert(std::abs(glassHit.thicknessMeters-0.04)<0.002);
    assert(glassHit.exitT>glassHit.t);
    assert(glassHit.normal.x<-0.99&&std::abs(glassHit.normal.y)<1e-9&&std::abs(glassHit.normal.z)<1e-9);

    const auto brickHit=world.raycastSegment({25,1,-19.5},{30,1,-19.5});
    assert(brickHit.hit&&brickHit.material==WorldMaterial::Brick);
    assert(std::abs(brickHit.thicknessMeters-0.12)<0.002);
    assert(brickHit.exitT>brickHit.t);

    for(WorldMaterial material:{WorldMaterial::Concrete,WorldMaterial::Steel,WorldMaterial::Wood,WorldMaterial::Brick,WorldMaterial::Glass,WorldMaterial::Soil,WorldMaterial::Rock})
        assert(MaterialCore::validateProfile(MaterialCore::ballistic(material)));
    assert(MaterialCore::ballistic(WorldMaterial::Glass).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Brick).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Wood).penetrable);
    assert(!MaterialCore::ballistic(WorldMaterial::Steel).penetrable);
    assert(MaterialCore::ballistic(WorldMaterial::Steel).ricochetEligible);
    assert(!MaterialCore::ballistic(WorldMaterial::Soil).ricochetEligible);

    DamageCore glassDamage;
    BallisticsCore glassBallistics;
    ShotSolution glassShot{};
    glassShot.origin={-35,1,17};
    glassShot.direction={1,0,0};
    glassShot.muzzleVelocity=820;
    glassShot.massKg=.004;
    glassShot.correlationId=7001;
    assert(glassBallistics.spawn(glassShot));
    glassBallistics.fixedStep(1.0/60.0,world,glassDamage);
    assert(glassBallistics.metrics().worldImpacts==1);
    assert(glassBallistics.metrics().penetrations==1);
    assert(glassBallistics.metrics().terminalWorldImpacts==0);
    assert(glassBallistics.activeCount()==1);
    assert(glassBallistics.validate());

    DamageCore brickDamage;
    BallisticsCore brickBallistics;
    ShotSolution brickShot{};
    brickShot.origin={25,1,-19.5};
    brickShot.direction={1,0,0};
    brickShot.muzzleVelocity=820;
    brickShot.massKg=.004;
    brickShot.correlationId=7002;
    assert(brickBallistics.spawn(brickShot));
    brickBallistics.fixedStep(1.0/60.0,world,brickDamage);
    assert(brickBallistics.metrics().worldImpacts==1);
    assert(brickBallistics.metrics().penetrations==1);
    assert(brickBallistics.metrics().terminalWorldImpacts==0);
    assert(brickBallistics.activeCount()==1);
    assert(brickBallistics.validate());

    DamageCore steelDamage;
    BallisticsCore steelBallistics;
    ShotSolution steelShot{};
    steelShot.origin={-16,1,9};
    steelShot.direction=normalize({0.2,0,1.0});
    steelShot.muzzleVelocity=820;
    steelShot.massKg=.004;
    steelShot.correlationId=7003;
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

    DamageCore concreteDamage;
    BallisticsCore concreteBallistics;
    ShotSolution concreteShot{};
    concreteShot.origin={5,1,14};
    concreteShot.direction={1,0,0};
    concreteShot.muzzleVelocity=820;
    concreteShot.massKg=.004;
    concreteShot.correlationId=7004;
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
