#include "METSEObservatoryCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {

void ObservatoryCore::reset() noexcept {
    *this=ObservatoryCore{};
}

void ObservatoryCore::observe(const ObservatoryFrameInput& i) noexcept {
    const bool finite=std::isfinite(i.realDeltaSeconds) && i.realDeltaSeconds>=0.0 &&
                      std::isfinite(i.simulationSliceMilliseconds) && i.simulationSliceMilliseconds>=0.0 &&
                      std::isfinite(i.playerX) && std::isfinite(i.playerZ) &&
                      std::isfinite(i.horizontalSpeed) && i.horizontalSpeed>=0.0 &&
                      std::isfinite(i.adsAlpha);
    if(!finite){
        ++rejectedSamples_;
        ++rejectedNonFiniteSamples_;
        return;
    }

    const std::uint64_t visibilityTotal=static_cast<std::uint64_t>(i.visibilityFull)+i.visibilityReduced+i.visibilityMinimal+i.visibilityDormant;
    const bool inRange=i.adsAlpha>=0.0 && i.adsAlpha<=1.0 &&
                       i.inputQueueDepth<=kMaxInputQueueDepth && i.activeProjectiles<=kMaxProjectiles &&
                       i.aiActiveAgents<=kMaxCombatants && i.aiLOSAgents<=i.aiActiveAgents &&
                       visibilityTotal<=kMaxCombatants;
    if(!inRange){
        ++rejectedSamples_;
        ++rejectedRangeSamples_;
        return;
    }
    if(hasPreviousTick_ && i.simulationTick<previousSimulationTick_){
        ++rejectedSamples_;
        ++simulationTickRegressions_;
        return;
    }

    const double ms=i.realDeltaSeconds*1000.0;
    ObservatoryFrame f{};
    f.simulationTick=i.simulationTick;
    f.frameMilliseconds=ms;
    f.simulationSliceMilliseconds=i.simulationSliceMilliseconds;
    f.horizontalSpeed=i.horizontalSpeed;
    f.catchUpSteps=i.catchUpSteps;
    f.collisionContacts=i.collisionContacts;
    f.inputQueueDepth=i.inputQueueDepth;
    f.activeProjectiles=i.activeProjectiles;
    f.aiActiveAgents=i.aiActiveAgents;
    f.aiLOSAgents=i.aiLOSAgents;
    f.aiDecisions=i.aiDecisions;
    f.projectileContacts=i.projectileContacts;
    f.projectileTerminalContacts=i.projectileTerminalContacts;
    f.projectileTargetContacts=i.projectileTargetContacts;
    f.stance=i.stance;
    f.gait=i.gait;
    f.grounded=i.grounded;
    f.sprinting=i.sprinting;
    f.catchUpClamped=i.catchUpClamped;

    frames_[frameWrite_]=f;
    frameWrite_=(frameWrite_+1)%kFrameCapacity;
    frameCount_=std::min(frameCount_+1,kFrameCapacity);
    ++observedFrames_;
    observedRealSeconds_+=i.realDeltaSeconds;
    sessionFrameTotalMilliseconds_+=ms;
    if(ms>20.0)++framesOver20ms_;
    if(ms>33.333333)++framesOver33ms_;
    if(i.simulationSliceMilliseconds>20.0)++simulationSlicesOver20ms_;
    if(i.catchUpClamped)++catchUpClampedFrames_;
    totalCollisionContacts_+=i.collisionContacts;
    totalProjectileContacts_+=i.projectileContacts;
    totalProjectileTerminalContacts_+=i.projectileTerminalContacts;
    totalProjectileTargetContacts_+=i.projectileTargetContacts;
    simulationSliceTotalMilliseconds_+=i.simulationSliceMilliseconds;
    maxSimulationSliceMilliseconds_=std::max(maxSimulationSliceMilliseconds_,i.simulationSliceMilliseconds);
    peakHorizontalSpeed_=std::max(peakHorizontalSpeed_,i.horizontalSpeed);

    if(hasPreviousPosition_){
        const double d=std::hypot(i.playerX-previousX_,i.playerZ-previousZ_);
        if(std::isfinite(d) && d<5.0)distanceTravelled_+=d;
        if(i.stance!=previousStance_)++stanceTransitions_;
        if(i.gait!=previousGait_)++gaitTransitions_;
    }
    previousX_=i.playerX;
    previousZ_=i.playerZ;
    previousStance_=i.stance;
    previousGait_=i.gait;
    previousSimulationTick_=i.simulationTick;
    hasPreviousPosition_=true;
    hasPreviousTick_=true;
    if(i.sprinting)sprintSeconds_+=i.realDeltaSeconds;
    if(!i.grounded)airborneSeconds_+=i.realDeltaSeconds;
    switch(i.stance){
        case CharacterStance::Standing:standingSeconds_+=i.realDeltaSeconds;break;
        case CharacterStance::Crouched:crouchedSeconds_+=i.realDeltaSeconds;break;
        case CharacterStance::Prone:proneSeconds_+=i.realDeltaSeconds;break;
    }

    latestQueueDepth_=i.inputQueueDepth;
    peakQueueDepth_=std::max(peakQueueDepth_,i.inputQueueDepth);
    latestProjectiles_=i.activeProjectiles;
    peakProjectiles_=std::max(peakProjectiles_,i.activeProjectiles);
    latestVisibilityFull_=i.visibilityFull;
    latestVisibilityReduced_=i.visibilityReduced;
    latestVisibilityMinimal_=i.visibilityMinimal;
    latestVisibilityDormant_=i.visibilityDormant;
    peakVisibilityFull_=std::max(peakVisibilityFull_,i.visibilityFull);
    peakVisibilityReduced_=std::max(peakVisibilityReduced_,i.visibilityReduced);
    peakVisibilityMinimal_=std::max(peakVisibilityMinimal_,i.visibilityMinimal);
    peakVisibilityDormant_=std::max(peakVisibilityDormant_,i.visibilityDormant);
    latestAIActiveAgents_=i.aiActiveAgents;
    latestAILOSAgents_=i.aiLOSAgents;
    latestAIDecisions_=i.aiDecisions;
    peakAIActiveAgents_=std::max(peakAIActiveAgents_,i.aiActiveAgents);
    peakAILOSAgents_=std::max(peakAILOSAgents_,i.aiLOSAgents);
    latestAmmo_=i.ammoInMagazine;
    latestReserveAmmo_=i.reserveAmmo;
    latestAdsAlpha_=i.adsAlpha;
    latestReloading_=i.reloading;
    latestWeaponObstructed_=i.weaponObstructed;
}

ObservatoryReport ObservatoryCore::report() const noexcept {
    ObservatoryReport o{};
    o.observedFrames=observedFrames_;
    o.retainedFrames=frameCount_;
    o.rejectedSamples=rejectedSamples_;
    o.rejectedNonFiniteSamples=rejectedNonFiniteSamples_;
    o.rejectedRangeSamples=rejectedRangeSamples_;
    o.simulationTickRegressions=simulationTickRegressions_;
    o.observedRealSeconds=observedRealSeconds_;
    o.framesOver20ms=framesOver20ms_;
    o.framesOver33ms=framesOver33ms_;
    o.simulationSlicesOver20ms=simulationSlicesOver20ms_;
    o.catchUpClampedFrames=catchUpClampedFrames_;
    o.totalCollisionContacts=totalCollisionContacts_;
    o.totalProjectileContacts=totalProjectileContacts_;
    o.totalProjectileTerminalContacts=totalProjectileTerminalContacts_;
    o.totalProjectileTargetContacts=totalProjectileTargetContacts_;
    o.distanceTravelled=distanceTravelled_;
    o.peakHorizontalSpeed=peakHorizontalSpeed_;
    o.sprintSeconds=sprintSeconds_;
    o.airborneSeconds=airborneSeconds_;
    o.standingSeconds=standingSeconds_;
    o.crouchedSeconds=crouchedSeconds_;
    o.proneSeconds=proneSeconds_;
    o.stanceTransitions=stanceTransitions_;
    o.gaitTransitions=gaitTransitions_;
    o.latestQueueDepth=latestQueueDepth_;
    o.peakQueueDepth=peakQueueDepth_;
    o.latestProjectiles=latestProjectiles_;
    o.peakProjectiles=peakProjectiles_;
    o.latestVisibilityFull=latestVisibilityFull_;
    o.latestVisibilityReduced=latestVisibilityReduced_;
    o.latestVisibilityMinimal=latestVisibilityMinimal_;
    o.latestVisibilityDormant=latestVisibilityDormant_;
    o.peakVisibilityFull=peakVisibilityFull_;
    o.peakVisibilityReduced=peakVisibilityReduced_;
    o.peakVisibilityMinimal=peakVisibilityMinimal_;
    o.peakVisibilityDormant=peakVisibilityDormant_;
    o.latestAIActiveAgents=latestAIActiveAgents_;
    o.latestAILOSAgents=latestAILOSAgents_;
    o.latestAIDecisions=latestAIDecisions_;
    o.peakAIActiveAgents=peakAIActiveAgents_;
    o.peakAILOSAgents=peakAILOSAgents_;
    o.latestAmmo=latestAmmo_;
    o.latestReserveAmmo=latestReserveAmmo_;
    o.latestAdsAlpha=latestAdsAlpha_;
    o.latestReloading=latestReloading_;
    o.latestWeaponObstructed=latestWeaponObstructed_;
    if(observedFrames_>0){
        o.sessionAverageFrameMilliseconds=sessionFrameTotalMilliseconds_/static_cast<double>(observedFrames_);
        o.sessionAverageSimulationSliceMilliseconds=simulationSliceTotalMilliseconds_/static_cast<double>(observedFrames_);
    }
    if(!frameCount_)return o;

    std::array<double,kFrameCapacity>frameValues{};
    double frameSum=0.0;
    double sliceSum=0.0;
    double retainedRealMilliseconds=0.0;
    double maxSlice=0.0;
    const std::size_t oldest=(frameWrite_+kFrameCapacity-frameCount_)%kFrameCapacity;
    std::uint64_t oldestTick=0;
    std::uint64_t newestTick=0;
    for(std::size_t n=0;n<frameCount_;++n){
        const ObservatoryFrame& f=frames_[(oldest+n)%kFrameCapacity];
        frameValues[n]=f.frameMilliseconds;
        frameSum+=f.frameMilliseconds;
        sliceSum+=f.simulationSliceMilliseconds;
        retainedRealMilliseconds+=f.frameMilliseconds;
        maxSlice=std::max(maxSlice,f.simulationSliceMilliseconds);
        if(n==0)oldestTick=f.simulationTick;
        newestTick=f.simulationTick;
        if(f.frameMilliseconds>20.0)++o.windowFramesOver20ms;
        if(f.frameMilliseconds>33.333333)++o.windowFramesOver33ms;
        if(f.simulationSliceMilliseconds>20.0)++o.windowSimulationSlicesOver20ms;
        if(f.catchUpClamped)++o.windowCatchUpClampedFrames;
    }
    std::sort(frameValues.begin(),frameValues.begin()+static_cast<std::ptrdiff_t>(frameCount_));
    const auto percentile=[&](double p){
        const std::size_t index=std::min(frameCount_-1,static_cast<std::size_t>(std::ceil(frameCount_*p))-1);
        return frameValues[index];
    };
    o.retainedRealSeconds=retainedRealMilliseconds/1000.0;
    o.retainedSimulationTicks=newestTick>=oldestTick ? newestTick-oldestTick : 0;
    o.averageFrameMilliseconds=frameSum/static_cast<double>(frameCount_);
    o.p95FrameMilliseconds=percentile(0.95);
    o.p99FrameMilliseconds=percentile(0.99);
    o.maxFrameMilliseconds=frameValues[frameCount_-1];
    o.averageSimulationSliceMilliseconds=sliceSum/static_cast<double>(frameCount_);
    o.maxSimulationSliceMilliseconds=maxSlice;
    if(o.averageFrameMilliseconds>1e-6)o.estimatedFPS=1000.0/o.averageFrameMilliseconds;
    if(o.p99FrameMilliseconds>1e-6)o.onePercentLowFPS=1000.0/o.p99FrameMilliseconds;
    const double p999=percentile(0.999);
    if(p999>1e-6)o.pointOnePercentLowFPS=1000.0/p999;
    return o;
}

bool ObservatoryCore::newestFrame(std::size_t off,ObservatoryFrame&out) const noexcept {
    if(off>=frameCount_)return false;
    out=frames_[(frameWrite_+kFrameCapacity-1-off)%kFrameCapacity];
    return true;
}

bool ObservatoryCore::validate() const noexcept {
    if(frameCount_>kFrameCapacity || frameWrite_>=kFrameCapacity ||
       !std::isfinite(observedRealSeconds_) || observedRealSeconds_<0.0 ||
       !std::isfinite(sessionFrameTotalMilliseconds_) || sessionFrameTotalMilliseconds_<0.0 ||
       !std::isfinite(simulationSliceTotalMilliseconds_) || simulationSliceTotalMilliseconds_<0.0 ||
       !std::isfinite(maxSimulationSliceMilliseconds_) || maxSimulationSliceMilliseconds_<0.0 ||
       !std::isfinite(distanceTravelled_) || distanceTravelled_<0.0 ||
       !std::isfinite(peakHorizontalSpeed_) || peakHorizontalSpeed_<0.0 ||
       !std::isfinite(latestAdsAlpha_) || latestAdsAlpha_<0.0 || latestAdsAlpha_>1.0 ||
       peakQueueDepth_>kMaxInputQueueDepth || peakProjectiles_>kMaxProjectiles ||
       peakAIActiveAgents_>kMaxCombatants || peakAILOSAgents_>kMaxCombatants ||
       peakAILOSAgents_>peakAIActiveAgents_){
        return false;
    }
    std::uint64_t previousTick=0;
    bool hasTick=false;
    for(std::size_t i=0;i<frameCount_;++i){
        ObservatoryFrame f{};
        if(!newestFrame(i,f) || !std::isfinite(f.frameMilliseconds) || f.frameMilliseconds<0.0 ||
           !std::isfinite(f.simulationSliceMilliseconds) || f.simulationSliceMilliseconds<0.0 ||
           !std::isfinite(f.horizontalSpeed) || f.horizontalSpeed<0.0 ||
           f.activeProjectiles>kMaxProjectiles || f.aiActiveAgents>kMaxCombatants ||
           f.aiLOSAgents>f.aiActiveAgents || f.inputQueueDepth>kMaxInputQueueDepth){
            return false;
        }
        if(hasTick && f.simulationTick>previousTick)return false;
        previousTick=f.simulationTick;
        hasTick=true;
    }
    return true;
}

} // namespace metse
