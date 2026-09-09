#pragma once
#include "METSECharacterMotor.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace metse {
struct ObservatoryFrameInput{std::uint64_t simulationTick=0;double realDeltaSeconds=0,simulationSliceMilliseconds=0,playerX=0,playerZ=0,horizontalSpeed=0;CharacterStance stance=CharacterStance::Standing;CharacterGait gait=CharacterGait::Idle;bool grounded=true,sprinting=false;std::uint32_t catchUpSteps=0;bool catchUpClamped=false;std::uint32_t collisionContacts=0,inputQueueDepth=0,activeProjectiles=0,visibilityFull=0,visibilityReduced=0,visibilityMinimal=0,visibilityDormant=0,aiActiveAgents=0,aiLOSAgents=0,aiDecisions=0;std::uint64_t projectileContacts=0,projectileTerminalContacts=0,projectileTargetContacts=0;std::uint32_t ammoInMagazine=0,reserveAmmo=0;double adsAlpha=0;bool reloading=false,weaponObstructed=false;};
struct ObservatoryFrame{std::uint64_t simulationTick=0;double frameMilliseconds=0,simulationSliceMilliseconds=0,horizontalSpeed=0;std::uint32_t catchUpSteps=0,collisionContacts=0,inputQueueDepth=0,activeProjectiles=0,aiActiveAgents=0,aiLOSAgents=0,aiDecisions=0;std::uint64_t projectileContacts=0,projectileTerminalContacts=0,projectileTargetContacts=0;CharacterStance stance=CharacterStance::Standing;CharacterGait gait=CharacterGait::Idle;bool grounded=true,sprinting=false,catchUpClamped=false;};
struct ObservatoryReport {
    std::uint64_t observedFrames=0;
    std::size_t retainedFrames=0;
    std::uint64_t rejectedSamples=0;
    std::uint64_t rejectedNonFiniteSamples=0;
    std::uint64_t rejectedRangeSamples=0;
    std::uint64_t simulationTickRegressions=0;
    double observedRealSeconds=0;
    double retainedRealSeconds=0;
    std::uint64_t retainedSimulationTicks=0;
    double sessionAverageFrameMilliseconds=0;
    double averageFrameMilliseconds=0,p95FrameMilliseconds=0,p99FrameMilliseconds=0,maxFrameMilliseconds=0;
    double averageSimulationSliceMilliseconds=0,maxSimulationSliceMilliseconds=0;
    double sessionAverageSimulationSliceMilliseconds=0;
    double estimatedFPS=0,onePercentLowFPS=0,pointOnePercentLowFPS=0;
    std::uint64_t framesOver20ms=0,framesOver33ms=0,simulationSlicesOver20ms=0,catchUpClampedFrames=0;
    std::uint64_t windowFramesOver20ms=0,windowFramesOver33ms=0,windowSimulationSlicesOver20ms=0,windowCatchUpClampedFrames=0;
    std::uint64_t totalCollisionContacts=0,totalProjectileContacts=0,totalProjectileTerminalContacts=0,totalProjectileTargetContacts=0;
    double distanceTravelled=0,peakHorizontalSpeed=0,sprintSeconds=0,airborneSeconds=0,standingSeconds=0,crouchedSeconds=0,proneSeconds=0;
    std::uint64_t stanceTransitions=0,gaitTransitions=0;
    std::uint32_t latestQueueDepth=0,peakQueueDepth=0,latestProjectiles=0,peakProjectiles=0;
    std::uint32_t latestVisibilityFull=0,latestVisibilityReduced=0,latestVisibilityMinimal=0,latestVisibilityDormant=0;
    std::uint32_t peakVisibilityFull=0,peakVisibilityReduced=0,peakVisibilityMinimal=0,peakVisibilityDormant=0;
    std::uint32_t latestAIActiveAgents=0,latestAILOSAgents=0,latestAIDecisions=0,peakAIActiveAgents=0,peakAILOSAgents=0;
    std::uint32_t latestAmmo=0,latestReserveAmmo=0;
    double latestAdsAlpha=0;
    bool latestReloading=false,latestWeaponObstructed=false;
};

class ObservatoryCore final {
public:
    static constexpr std::size_t kFrameCapacity=600;
    static constexpr std::uint32_t kMaxInputQueueDepth=64;
    static constexpr std::uint32_t kMaxProjectiles=128;
    static constexpr std::uint32_t kMaxCombatants=32;
    void reset()noexcept;
    void observe(const ObservatoryFrameInput& input)noexcept;
    [[nodiscard]]ObservatoryReport report()const noexcept;
    [[nodiscard]]bool newestFrame(std::size_t offset,ObservatoryFrame&out)const noexcept;
    [[nodiscard]]std::size_t retainedFrameCount()const noexcept{return frameCount_;}
    [[nodiscard]]bool validate()const noexcept;

private:
    std::array<ObservatoryFrame,kFrameCapacity>frames_{};
    std::size_t frameWrite_=0,frameCount_=0;
    std::uint64_t observedFrames_=0,rejectedSamples_=0,rejectedNonFiniteSamples_=0,rejectedRangeSamples_=0,simulationTickRegressions_=0;
    std::uint64_t framesOver20ms_=0,framesOver33ms_=0,simulationSlicesOver20ms_=0,catchUpClampedFrames_=0;
    std::uint64_t totalCollisionContacts_=0,totalProjectileContacts_=0,totalProjectileTerminalContacts_=0,totalProjectileTargetContacts_=0;
    double observedRealSeconds_=0,sessionFrameTotalMilliseconds_=0,simulationSliceTotalMilliseconds_=0,maxSimulationSliceMilliseconds_=0;
    double distanceTravelled_=0,peakHorizontalSpeed_=0,sprintSeconds_=0,airborneSeconds_=0,standingSeconds_=0,crouchedSeconds_=0,proneSeconds_=0;
    std::uint64_t stanceTransitions_=0,gaitTransitions_=0;
    bool hasPreviousPosition_=false,hasPreviousTick_=false;
    double previousX_=0,previousZ_=0;
    std::uint64_t previousSimulationTick_=0;
    CharacterStance previousStance_=CharacterStance::Standing;
    CharacterGait previousGait_=CharacterGait::Idle;
    std::uint32_t latestQueueDepth_=0,peakQueueDepth_=0,latestProjectiles_=0,peakProjectiles_=0;
    std::uint32_t latestVisibilityFull_=0,latestVisibilityReduced_=0,latestVisibilityMinimal_=0,latestVisibilityDormant_=0;
    std::uint32_t peakVisibilityFull_=0,peakVisibilityReduced_=0,peakVisibilityMinimal_=0,peakVisibilityDormant_=0;
    std::uint32_t latestAIActiveAgents_=0,latestAILOSAgents_=0,latestAIDecisions_=0,peakAIActiveAgents_=0,peakAILOSAgents_=0;
    std::uint32_t latestAmmo_=0,latestReserveAmmo_=0;
    double latestAdsAlpha_=0;
    bool latestReloading_=false,latestWeaponObstructed_=false;
};
} // namespace metse
