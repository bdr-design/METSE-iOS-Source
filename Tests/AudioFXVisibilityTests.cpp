#include "../Engine/Core/METSEAudioFXCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEVisibilityCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main(){
    using namespace metse;

    WorldCollisionCore world;
    assert(world.validate());
    assert(world.surfacePatchCount()==WorldCollisionCore::kMaxSurfacePatches);
    assert(world.surfaceMaterialAt(30.0,-25.0)==WorldMaterial::Steel);
    assert(world.surfaceMaterialAt(-20.0,20.0)==WorldMaterial::Concrete);
    assert(world.surfaceMaterialAt(-10.0,-30.0)==WorldMaterial::Rock);
    assert(world.surfaceMaterialAt(5.0,0.0)==WorldMaterial::Soil);
    assert(world.hasOverheadCover({0.0,1.0,8.0},5.0));
    assert(!world.hasOverheadCover({5.0,1.0,0.0},5.0));

    AudioFXCore audio;
    assert(audio.validate());
    Vec3 previous{-44.0,0.0,10.0};
    for(int i=1;i<=7;++i){
        Vec3 current{-44.0+0.2*i,0.0,10.0};
        audio.observeMovement(previous,current,1.55,CharacterGait::Walk,world);
        previous=current;
    }
    auto report=audio.report();
    assert(report.footsteps==1);
    AudioCue footstep{};
    assert(audio.cueBySequence(1,footstep));
    assert(footstep.kind==AudioCueKind::Footstep);
    assert(footstep.material==WorldMaterial::Concrete);

    audio.observeShot({5.0,1.64,0.0},101,world);
    audio.observeShot({0.0,1.0,8.0},102,world);
    report=audio.report();
    assert(report.outdoorShots==1&&report.indoorShots==1);
    AudioCue outdoor{},indoor{};
    assert(audio.cueBySequence(2,outdoor));
    assert(audio.cueBySequence(3,indoor));
    assert(outdoor.kind==AudioCueKind::FireOutdoor&&!outdoor.indoor);
    assert(indoor.kind==AudioCueKind::FireIndoor&&indoor.indoor);

    const Vec3 listener{0.0,1.6,1.0};
    audio.observeProjectileSegment({-5.0,1.6,0.0},{5.0,1.6,0.0},listener,800.0,501,true);
    report=audio.report();
    assert(report.bulletCracks==1&&report.nearMisses==1);
    const auto cueCountAfterFirstPass=report.cuesEmitted;
    audio.observeProjectileSegment({-4.0,1.6,0.0},{4.0,1.6,0.0},listener,780.0,501,true);
    assert(audio.report().cuesEmitted==cueCountAfterFirstPass);
    audio.observeProjectileSegment({-5.0,1.6,0.0},{5.0,1.6,0.0},listener,800.0,502,false);
    assert(audio.report().cuesEmitted==cueCountAfterFirstPass);
    assert(audio.validate());

    AudioFXCore audioA,audioB;
    for(int i=0;i<12;++i){
        Vec3 a{-5.0+i*0.15,0.0,0.0};
        Vec3 b{-5.0+(i+1)*0.15,0.0,0.0};
        audioA.observeMovement(a,b,4.15,CharacterGait::Jog,world);
        audioB.observeMovement(a,b,4.15,CharacterGait::Jog,world);
        audioA.fixedStep(1.0/60.0);
        audioB.fixedStep(1.0/60.0);
    }
    audioA.observeShot({5.0,1.64,0.0},700,world);
    audioB.observeShot({5.0,1.64,0.0},700,world);
    assert(audioA.deterministicFingerprint()==audioB.deterministicFingerprint());
    assert(audioA.validate()&&audioB.validate());
    for(int i=0;i<120;++i) audio.fixedStep(1.0/60.0);
    assert(audio.report().activeFX==0);
    assert(audio.validate());

    VisibilityCore visibility;
    for(std::size_t i=0;i<VisibilityCore::kMaxEntities;++i){
        const double x=(static_cast<double>(i%8)-3.5)*0.5;
        const double z=20.0+static_cast<double>(i)*0.10;
        visibility.syncTarget(i,static_cast<std::uint32_t>(i+1),{x,0.0,z},true);
    }
    visibility.update({0.0,1.6,0.0},0.0);
    const auto budgeted=visibility.report();
    assert(budgeted.full==VisibilityCore::kFullBudget);
    assert(budgeted.reduced==VisibilityCore::kReducedBudget);
    assert(budgeted.minimal==VisibilityCore::kMinimalBudget);
    assert(budgeted.dormant==0);
    assert(budgeted.evaluated==VisibilityCore::kMaxEntities);
    assert(budgeted.budgetDemotions==24);
    assert(visibility.validate());

    VisibilityCore visibilityB;
    for(std::size_t i=0;i<VisibilityCore::kMaxEntities;++i){
        const double x=(static_cast<double>(i%8)-3.5)*0.5;
        const double z=20.0+static_cast<double>(i)*0.10;
        visibilityB.syncTarget(i,static_cast<std::uint32_t>(i+1),{x,0.0,z},true);
    }
    visibilityB.update({0.0,1.6,0.0},0.0);
    for(std::size_t i=0;i<VisibilityCore::kMaxEntities;++i)
        assert(visibility.entities()[i].tier==visibilityB.entities()[i].tier);

    VisibilityCore legacyVisibility;
    legacyVisibility.syncTarget(0,1,{0.0,0.0,20.0},true);
    legacyVisibility.syncTarget(1,2,{0.0,0.0,120.0},true);
    legacyVisibility.update({0.0,1.6,0.0},0.0);
    const auto legacyReport=legacyVisibility.report();
    assert(legacyReport.full==1&&legacyReport.minimal==1);
    assert(legacyVisibility.validate());

    // Engine integration: accepted simulation movement owns footsteps and the same
    // Fire transaction owns the acoustic/muzzle intent. Reset clears both pools.
    EngineCore engine;
    assert(engine.snapshot().audioFX.cuesEmitted==0);
    engine.setMovementInput(1.0,0.0);
    for(int i=0;i<24;++i) engine.advance(1.0/60.0);
    assert(engine.snapshot().audioFX.footsteps>=1);
    assert(engine.diagnostics().audioFXValid);
    engine.setMovementInput(0.0,0.0);
    for(int i=0;i<8;++i) engine.advance(1.0/60.0);
    const auto beforeShotCues=engine.snapshot().audioFX.cuesEmitted;
    assert(engine.triggerFire());
    engine.advance(1.0/60.0);
    assert(engine.snapshot().audioFX.outdoorShots==1);
    assert(engine.snapshot().audioFX.cuesEmitted==beforeShotCues+1);
    const auto fingerprintBeforeRollback=engine.audioFX().deterministicFingerprint();
    assert(!engine.testOnlyExecuteInvariantViolation());
    assert(engine.audioFX().deterministicFingerprint()==fingerprintBeforeRollback);
    assert(engine.diagnostics().audioFXValid);
    engine.reset();
    assert(engine.snapshot().audioFX.cuesEmitted==0);
    assert(engine.snapshot().audioFX.activeFX==0);

    EngineCore engineA,engineB;
    for(int i=0;i<90;++i){
        engineA.setMovementInput(0.7,0.15);
        engineB.setMovementInput(0.7,0.15);
        if(i==36){ assert(engineA.triggerFire()); assert(engineB.triggerFire()); }
        engineA.advance(1.0/60.0);
        engineB.advance(1.0/60.0);
    }
    assert(engineA.audioFX().deterministicFingerprint()==engineB.audioFX().deterministicFingerprint());
    assert(engineA.deterministicStateHash()==engineB.deterministicStateHash());

    std::cout<<"METSE Build 009-G Audio FX + Visibility Tests: PASS\n";
}
