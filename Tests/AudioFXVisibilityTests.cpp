#include "../Engine/Core/METSEAudioFXCore.hpp"
#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEVisibilityCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

using namespace metse;

struct SegmentObserverContext {
    AudioFXCore *audio=nullptr;
    Vec3 listener{};
    bool hostile=false;
    std::uint32_t observations=0;
};

void observeBallisticSegment(void *rawContext,
                             const ProjectileSegmentObservation& segment) noexcept {
    auto *context=static_cast<SegmentObserverContext *>(rawContext);
    if(context==nullptr||context->audio==nullptr) return;
    ++context->observations;
    context->audio->observeProjectileSegment(segment,context->listener,context->hostile);
}

std::uint64_t footstepsAcross(double distance,CharacterGait gait,bool grounded=true) {
    WorldCollisionCore world;
    AudioFXCore audio;
    constexpr double kIncrement=0.05;
    Vec3 previous{5.0,0.0,-5.0};
    const int steps=static_cast<int>(std::round(distance/kIncrement));
    for(int i=0;i<steps;++i){
        const Vec3 current{previous.x,previous.y,previous.z+kIncrement};
        audio.observeMovement(previous,current,6.0,gait,grounded,world);
        previous=current;
    }
    assert(audio.validate());
    return audio.report().footsteps;
}

void verifySurfaceIdentity(const WorldCollisionCore& world,
                           Vec3 center,
                           WorldMaterial expected) {
    const auto resolved=world.resolve(center.x,center.z,center.x,center.z,0.34,1.78);
    assert(std::abs(resolved.x-center.x)<1e-9);
    assert(std::abs(resolved.z-center.z)<1e-9);
    assert(world.surfaceMaterialAt(center.x,center.z)==expected);

    AudioFXCore audio;
    const Vec3 previous{center.x-0.60,center.y,center.z};
    const Vec3 current{center.x+0.60,center.y,center.z};
    audio.observeMovement(previous,current,1.55,CharacterGait::Walk,true,world);
    assert(audio.report().footsteps==1);
    AudioCue cue{};
    assert(audio.cueBySequence(1,cue));
    assert(cue.kind==AudioCueKind::Footstep);
    assert(cue.material==expected);
    assert(audio.validate());
}

ProjectileSegmentObservation segmentAtLateralDistance(double distance,
                                                       double speed,
                                                       std::uint64_t correlationId,
                                                       bool terminated=false) {
    return {{-5.0,1.6,distance},{5.0,1.6,distance},speed,correlationId,true,terminated};
}

void seedBudgetScenario(VisibilityCore& visibility) {
    for(std::size_t i=0;i<VisibilityCore::kMaxEntities;++i){
        const double z=-34.0+static_cast<double>(i)*0.5;
        visibility.syncTarget(i,static_cast<std::uint32_t>(i+1),{5.0,0.0,z},true);
    }
}

void runThirtyTwoCombatantAudioStress(AudioFXCore& audio,
                                      const WorldCollisionCore& world) {
    for(std::uint64_t i=0;i<32;++i){
        audio.observeShot({5.0,1.64,0.0},1000+i,world);
    }
    const Vec3 listener{0.0,1.6,0.0};
    for(std::uint64_t i=0;i<32;++i){
        audio.observeProjectileSegment(segmentAtLateralDistance(1.0,800.0,2000+i),
                                       listener,true);
    }
}

} // namespace

int main(){
    using namespace metse;

    static_assert(AudioFXCore::kCueCapacity==64);
    static_assert(AudioFXCore::kFXCapacity==48);
    static_assert(AudioFXCore::kProjectileCueMemoryCapacity==BallisticsCore::kMaxProjectiles);
    static_assert(VisibilityCore::kFullBudget+VisibilityCore::kReducedBudget+
                  VisibilityCore::kMinimalBudget==VisibilityCore::kMaxEntities);
    static_assert(WorldCollisionCore::kAcousticProbeRayCount==5);

    WorldCollisionCore world;
    assert(world.validate());
    assert(world.surfacePatchCount()==WorldCollisionCore::kMaxSurfacePatches);

    // Footstep material comes from reachable WorldCollision floor truth. Six explicit
    // patches plus the Soil fallback cover the complete seven-material SSOT.
    verifySurfaceIdentity(world,{39.0,0.0,-27.0},WorldMaterial::Steel);
    verifySurfaceIdentity(world,{-23.0,0.0,18.0},WorldMaterial::Concrete);
    verifySurfaceIdentity(world,{-41.0,0.0,-36.0},WorldMaterial::Rock);
    verifySurfaceIdentity(world,{23.0,0.0,17.0},WorldMaterial::Wood);
    verifySurfaceIdentity(world,{-12.0,0.0,38.0},WorldMaterial::Brick);
    verifySurfaceIdentity(world,{-42.0,0.0,-13.0},WorldMaterial::Glass);
    verifySurfaceIdentity(world,{5.0,0.0,0.0},WorldMaterial::Soil);

    // Stationary, non-step locomotion and airborne motion cannot emit footsteps.
    AudioFXCore stationary;
    for(int i=0;i<180;++i){
        stationary.observeMovement({5.0,0.0,0.0},{5.0,0.0,0.0},6.0,
                                   CharacterGait::Sprint,true,world);
    }
    stationary.observeMovement({5.0,0.0,0.0},{6.5,0.0,0.0},1.0,
                               CharacterGait::Idle,true,world);
    stationary.observeMovement({5.0,0.0,0.0},{6.5,0.0,0.0},1.0,
                               CharacterGait::Crawl,true,world);
    stationary.observeMovement({5.0,0.0,0.0},{6.5,0.0,0.0},6.0,
                               CharacterGait::Sprint,false,world);
    assert(stationary.report().footsteps==0);
    assert(stationary.validate());

    // Cadence is distance-based, deterministic and bounded by gait/stance truth.
    constexpr double kCadenceDistance=12.0;
    for(CharacterGait gait:{CharacterGait::Walk,CharacterGait::Tactical,
                            CharacterGait::Jog,CharacterGait::Sprint,CharacterGait::Crouch}){
        const auto spacing=AudioFXCore::footstepSpacingMeters(gait);
        const auto expected=static_cast<std::uint64_t>(std::floor((kCadenceDistance+1e-9)/spacing));
        const auto actual=footstepsAcross(kCadenceDistance,gait);
        if(actual!=expected){
            std::cerr<<"cadence mismatch gait="<<static_cast<int>(gait)
                     <<" expected="<<expected<<" actual="<<actual<<"\n";
        }
        assert(actual==expected);
    }
    assert(footstepsAcross(kCadenceDistance,CharacterGait::Sprint,false)==0);

    // Indoor/outdoor firing uses five bounded raycasts against the one world geometry.
    const Vec3 outdoorOrigin{5.0,1.64,0.0};
    const Vec3 indoorOrigin{0.0,1.0,8.0};
    const auto outdoorProbe=world.acousticProbeAt(outdoorOrigin);
    const auto indoorProbe=world.acousticProbeAt(indoorOrigin);
    assert(outdoorProbe.raysCast==WorldCollisionCore::kAcousticProbeRayCount);
    assert(!outdoorProbe.overheadBlocked&&!outdoorProbe.indoor);
    assert(indoorProbe.raysCast==WorldCollisionCore::kAcousticProbeRayCount);
    assert(indoorProbe.overheadBlocked&&indoorProbe.occludedRays>=3&&indoorProbe.indoor);
    for(int i=0;i<128;++i){
        const auto repeated=world.acousticProbeAt(indoorOrigin);
        assert(repeated.indoor==indoorProbe.indoor);
        assert(repeated.overheadBlocked==indoorProbe.overheadBlocked);
        assert(repeated.raysCast==indoorProbe.raysCast);
        assert(repeated.occludedRays==indoorProbe.occludedRays);
    }

    AudioFXCore layeredShots;
    layeredShots.observeShot(outdoorOrigin,101,world);
    layeredShots.observeShot(indoorOrigin,102,world);
    auto report=layeredShots.report();
    assert(report.outdoorShots==1&&report.indoorShots==1);
    assert(report.fxSpawned==2&&report.activeFX==2&&report.fxDropped==0);
    AudioCue outdoorCue{},indoorCue{};
    assert(layeredShots.cueBySequence(1,outdoorCue));
    assert(layeredShots.cueBySequence(2,indoorCue));
    assert(outdoorCue.kind==AudioCueKind::FireOutdoor&&!outdoorCue.indoor);
    assert(indoorCue.kind==AudioCueKind::FireIndoor&&indoorCue.indoor);

    // Crack and near-miss thresholds include the exact boundary and exclude the next
    // representable test point outside it. Correlation memory deduplicates ordering.
    const Vec3 listener{0.0,1.6,0.0};
    AudioFXCore crackBoundary;
    crackBoundary.observeProjectileSegment(
        segmentAtLateralDistance(AudioFXCore::kCrackRadiusMeters,
                                 AudioFXCore::kCrackMinimumSpeedMetersPerSecond,501),
        listener,true);
    assert(crackBoundary.report().bulletCracks==1);
    assert(crackBoundary.report().nearMisses==0);
    crackBoundary.observeProjectileSegment(segmentAtLateralDistance(0.0,800.0,501),listener,true);
    assert(crackBoundary.report().cuesEmitted==2);
    assert(crackBoundary.report().nearMisses==1);
    crackBoundary.observeProjectileSegment(segmentAtLateralDistance(0.0,800.0,501),listener,true);
    assert(crackBoundary.report().cuesEmitted==2);

    AudioFXCore crackOutside;
    crackOutside.observeProjectileSegment(
        segmentAtLateralDistance(AudioFXCore::kCrackRadiusMeters+1e-6,
                                 AudioFXCore::kCrackMinimumSpeedMetersPerSecond,502),
        listener,true);
    assert(crackOutside.report().bulletCracks==0);

    AudioFXCore nearBoundary;
    nearBoundary.observeProjectileSegment(
        segmentAtLateralDistance(AudioFXCore::kNearMissRadiusMeters,
                                 AudioFXCore::kNearMissMinimumSpeedMetersPerSecond,503),
        listener,true);
    assert(nearBoundary.report().nearMisses==1);
    assert(nearBoundary.report().bulletCracks==0);

    AudioFXCore nearOutside;
    nearOutside.observeProjectileSegment(
        segmentAtLateralDistance(AudioFXCore::kNearMissRadiusMeters+1e-6,
                                 AudioFXCore::kNearMissMinimumSpeedMetersPerSecond,504),
        listener,true);
    assert(nearOutside.report().nearMisses==0);
    nearOutside.observeProjectileSegment(segmentAtLateralDistance(0.0,79.999,505),listener,true);
    assert(nearOutside.report().nearMisses==0);
    nearOutside.observeProjectileSegment(segmentAtLateralDistance(0.0,800.0,506),listener,false);
    assert(nearOutside.report().cuesEmitted==0);

    // A terminatedAfterSegment observation closes its correlation even if the segment was
    // far away. Stray post-termination observations cannot create a late near miss.
    AudioFXCore terminated;
    terminated.observeProjectileSegment(
        segmentAtLateralDistance(20.0,800.0,601,true),listener,true);
    assert(terminated.report().cuesEmitted==0);
    terminated.observeProjectileSegment(segmentAtLateralDistance(0.0,800.0,601),listener,true);
    assert(terminated.report().cuesEmitted==0);
    terminated.observeProjectileSegment({{0.0,1.6,0.0},{0.0,1.6,0.0},0.0,602,false,true},
                                        listener,true);
    terminated.observeProjectileSegment(segmentAtLateralDistance(0.0,800.0,602),listener,true);
    assert(terminated.report().cuesEmitted==0);
    assert(terminated.validate());

    // The observer receives actual BallisticsCore trajectory segments without a
    // trajectory copy or secondary queue. The test marks this synthetic source hostile;
    // production player-owned projectiles remain explicitly non-hostile to the player.
    AudioFXCore observedBallistics;
    BallisticsCore ballistics;
    DamageCore damage;
    ShotSolution hostileTestShot{};
    hostileTestShot.origin={-10.0,1.6,-44.0};
    hostileTestShot.direction={1.0,0.0,0.0};
    hostileTestShot.aimPoint={100.0,1.6,-44.0};
    hostileTestShot.muzzleVelocity=800.0;
    hostileTestShot.massKg=0.004;
    hostileTestShot.correlationId=700;
    assert(ballistics.spawn(hostileTestShot));
    SegmentObserverContext observer{&observedBallistics,{0.0,1.6,-43.0},true,0};
    ballistics.fixedStep(1.0/60.0,world,damage,&observeBallisticSegment,&observer);
    assert(observer.observations==1);
    assert(observedBallistics.report().bulletCracks==1);
    assert(observedBallistics.report().nearMisses==1);
    assert(observedBallistics.validate());

    // FX saturation is fixed at 48 and uses deterministic drop-new semantics.
    AudioFXCore saturatedFX;
    for(std::uint64_t i=0;i<60;++i){
        saturatedFX.observeShot(outdoorOrigin,800+i,world);
    }
    report=saturatedFX.report();
    assert(report.fxSpawnRequests==60);
    assert(report.fxSpawned==AudioFXCore::kFXCapacity);
    assert(report.fxDropped==12);
    assert(report.activeFX==AudioFXCore::kFXCapacity);
    assert(report.retainedCues==60);
    saturatedFX.fixedStep(1.0);
    assert(saturatedFX.report().activeFX==0);
    saturatedFX.observeShot(outdoorOrigin,900,world);
    assert(saturatedFX.report().fxSpawned==AudioFXCore::kFXCapacity+1);
    assert(saturatedFX.report().activeFX==1);
    assert(saturatedFX.validate());

    // Cue ring overwrite is bounded and sequence-addressable without blocking.
    for(std::uint64_t i=0;i<10;++i){
        saturatedFX.observeShot(outdoorOrigin,910+i,world);
    }
    assert(saturatedFX.report().retainedCues==AudioFXCore::kCueCapacity);
    AudioCue retained{};
    assert(!saturatedFX.cueBySequence(1,retained));
    assert(saturatedFX.cueBySequence(saturatedFX.latestCueSequence(),retained));

    // 32-source stress saturates the cue ring exactly as specified and remains
    // deterministic in both totals and event ordering.
    AudioFXCore stressA,stressB;
    runThirtyTwoCombatantAudioStress(stressA,world);
    runThirtyTwoCombatantAudioStress(stressB,world);
    const auto stressReport=stressA.report();
    assert(stressReport.outdoorShots==32);
    assert(stressReport.bulletCracks==32&&stressReport.nearMisses==32);
    assert(stressReport.cuesEmitted==96&&stressReport.retainedCues==AudioFXCore::kCueCapacity);
    assert(stressReport.fxSpawned==32&&stressReport.fxDropped==0);
    AudioCue firstRetained{},secondRetained{},latestRetained{};
    assert(stressA.cueBySequence(33,firstRetained));
    assert(stressA.cueBySequence(34,secondRetained));
    assert(stressA.cueBySequence(96,latestRetained));
    assert(firstRetained.kind==AudioCueKind::BulletCrack&&firstRetained.correlationId==2000);
    assert(secondRetained.kind==AudioCueKind::BulletNearMiss&&secondRetained.correlationId==2000);
    assert(latestRetained.kind==AudioCueKind::BulletNearMiss&&latestRetained.correlationId==2031);
    assert(stressA.deterministicFingerprint()==stressB.deterministicFingerprint());
    assert(stressA.validate()&&stressB.validate());

    // Visibility distance boundaries are strict: 35 -> Reduced, 80 -> Minimal,
    // 150 -> Dormant. The x=5 open lane keeps geometry out of this boundary test.
    VisibilityCore boundaries;
    boundaries.syncTarget(0,1,{5.0,0.0,-9.0},true);
    boundaries.syncTarget(1,2,{5.0,0.0,36.0},true);
    boundaries.syncTarget(2,3,{5.0,0.0,106.0},true);
    boundaries.update({5.0,1.6,-44.0},0.0,world);
    assert(boundaries.entities()[0].tier==VisibilityTier::Reduced);
    assert(boundaries.entities()[1].tier==VisibilityTier::Minimal);
    assert(boundaries.entities()[2].tier==VisibilityTier::Dormant);
    assert(boundaries.report().evaluated==2&&boundaries.report().occluded==0);
    assert(boundaries.validate());

    // WorldCollision LOS belongs to VisibilityCore. Occluded targets are Dormant and
    // Metal receives that immutable tier instead of running renderer-only visibility.
    VisibilityCore occlusion;
    occlusion.syncTarget(0,10,{8.0,0.0,20.0},true);
    occlusion.syncTarget(1,11,{-10.0,0.0,18.0},true);
    occlusion.update({0.0,1.6,0.0},0.0,world);
    assert(occlusion.entities()[0].tier==VisibilityTier::Dormant);
    assert(!occlusion.entities()[0].lineOfSight);
    assert(occlusion.entities()[1].tier==VisibilityTier::Full);
    assert(occlusion.entities()[1].lineOfSight);
    assert(occlusion.report().occluded==1);
    assert(occlusion.validate());

    VisibilityCore budgetA,budgetB;
    seedBudgetScenario(budgetA);
    seedBudgetScenario(budgetB);
    budgetA.update({5.0,1.6,-44.0},0.0,world);
    budgetB.update({5.0,1.6,-44.0},0.0,world);
    const auto budgeted=budgetA.report();
    assert(budgeted.full==VisibilityCore::kFullBudget);
    assert(budgeted.reduced==VisibilityCore::kReducedBudget);
    assert(budgeted.minimal==VisibilityCore::kMinimalBudget);
    assert(budgeted.dormant==0&&budgeted.occluded==0);
    assert(budgeted.evaluated==VisibilityCore::kMaxEntities);
    assert(budgeted.budgetDemotions==24);
    for(std::size_t i=0;i<VisibilityCore::kMaxEntities;++i){
        assert(budgetA.entities()[i].tier==budgetB.entities()[i].tier);
        assert(budgetA.entities()[i].lineOfSight==budgetB.entities()[i].lineOfSight);
    }
    assert(budgetA.validate()&&budgetB.validate());

    // Engine integration: accepted simulation movement owns footsteps, Fire owns
    // shot/FX intent, self-owned projectile segments cannot create player near misses,
    // and rollback/reset restore the complete bounded subsystem.
    EngineCore engine;
    assert(engine.snapshot().audioFX.cuesEmitted==0);
    engine.setMovementInput(1.0,0.0);
    for(int i=0;i<30;++i) engine.advance(1.0/60.0);
    assert(engine.snapshot().audioFX.footsteps>=1);
    assert(engine.diagnostics().audioFXValid);
    engine.setMovementInput(0.0,0.0);
    for(int i=0;i<8;++i) engine.advance(1.0/60.0);
    const auto beforeShotCues=engine.snapshot().audioFX.cuesEmitted;
    assert(engine.triggerFire());
    engine.advance(1.0/60.0);
    assert(engine.snapshot().audioFX.outdoorShots==1);
    assert(engine.snapshot().audioFX.cuesEmitted==beforeShotCues+1);
    for(int i=0;i<12;++i) engine.advance(1.0/60.0);
    assert(engine.snapshot().audioFX.bulletCracks==0);
    assert(engine.snapshot().audioFX.nearMisses==0);
    const auto fingerprintBeforeRollback=engine.audioFX().deterministicFingerprint();
    assert(!engine.testOnlyExecuteInvariantViolation());
    assert(engine.audioFX().deterministicFingerprint()==fingerprintBeforeRollback);
    assert(engine.diagnostics().audioFXValid);
    engine.reset();
    assert(engine.snapshot().audioFX.cuesEmitted==0);
    assert(engine.snapshot().audioFX.activeFX==0);

    EngineCore engineA,engineB;
    for(int i=0;i<120;++i){
        assert(engineA.setMovementInput(0.7,0.15));
        assert(engineB.setMovementInput(0.7,0.15));
        if(i==36){
            assert(engineA.triggerFire());
            assert(engineB.triggerFire());
        }
        engineA.advance(1.0/60.0);
        engineB.advance(1.0/60.0);
    }
    assert(engineA.audioFX().deterministicFingerprint()==engineB.audioFX().deterministicFingerprint());
    assert(engineA.deterministicStateHash()==engineB.deterministicStateHash());

    std::cout<<"METSE Build 009-G Audio FX + Visibility Tests: PASS\n";
}
