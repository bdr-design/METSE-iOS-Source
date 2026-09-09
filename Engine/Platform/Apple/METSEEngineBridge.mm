#import "METSEEngineBridge.h"
#import "METSEAudioPresenter.h"
#import <QuartzCore/QuartzCore.h>
#import <os/lock.h>
#import <simd/simd.h>
#import <UIKit/UIKit.h>
#include "../../Core/METSEEngineCore.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

static constexpr NSUInteger kRenderObstacleCap = metse::WorldCollisionCore::kMaxObstacles;
static constexpr NSUInteger kRenderProjectileCap = 8;
static constexpr NSUInteger kRenderTargetCap = metse::VisibilityCore::kMaxEntities;
static constexpr NSUInteger kRenderFXCap = 16;
static_assert(kRenderFXCap <= metse::AudioFXCore::kFXCapacity,
              "Presentation FX budget cannot exceed the simulation-owned pool");

struct METSEFrameUniforms {
    vector_float4 timing;
    vector_float4 camera;
    vector_float4 character;
    vector_float4 weapon;
    vector_float4 weapon2;
    vector_float4 worldMeta;
    vector_float4 worldExtra;
    vector_float4 presentationMeta;
    vector_float4 obstacleBounds[kRenderObstacleCap];
    vector_float4 obstacleMeta[kRenderObstacleCap];
    vector_float4 projectilePositions[kRenderProjectileCap];
    vector_float4 targetData[kRenderTargetCap];
    vector_float4 targetMeta[kRenderTargetCap];
    vector_float4 fxData[kRenderFXCap];
    vector_float4 fxMeta[kRenderFXCap];
};
static_assert(sizeof(METSEFrameUniforms) % 16 == 0, "Metal uniform ABI must remain 16-byte aligned");
static_assert(sizeof(METSEFrameUniforms) <= 4096,
              "setFragmentBytes payload must remain within Metal's small-inline limit");

@interface METSEEngineBridge ()
@property(nonatomic, weak) MTKView *metalView;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@end

@implementation METSEEngineBridge {
    metse::EngineCore _core;
    METSEAudioPresenter *_audioPresenter;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _nextThermalCheck;
    BOOL _running;
    uint64_t _lastConsumedAudioCueSequence;
    uint64_t _audioCueSnapshotDrops;
    os_unfair_lock _coreLock;
    os_unfair_lock _telemetryLock;
    uint64_t _renderedFrames;
    uint64_t _drawableMisses;
    double _renderCpuTotalMilliseconds;
    double _renderCpuMaxMilliseconds;
    uint64_t _coreLockSamples;
    double _coreLockWaitTotalMilliseconds;
    double _coreLockWaitMaxMilliseconds;
    double _coreCriticalTotalMilliseconds;
    double _coreCriticalMaxMilliseconds;
    double _callbackGapMaxMilliseconds;
    uint64_t _callbackGapsOver250ms;
    uint64_t _lifecycleTimingResets;
    BOOL _suppressNextFrameGap;
    NSInteger _presentationFPS;
}

- (instancetype)initWithView:(MTKView *)view {
    self = [super init];
    if (!self) return nil;
    _metalView = view;
    _lastFrameTime = CACurrentMediaTime();
    _nextThermalCheck = _lastFrameTime;
    _coreLock = OS_UNFAIR_LOCK_INIT;
    _telemetryLock = OS_UNFAIR_LOCK_INIT;
    _presentationFPS = 60;
    _suppressNextFrameGap = NO;
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(handleTimingBoundary:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(handleTimingBoundary:) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [center addObserver:self selector:@selector(handleTimingBoundary:) name:UIApplicationWillEnterForegroundNotification object:nil];
    [center addObserver:self selector:@selector(handleTimingBoundary:) name:UIApplicationDidBecomeActiveNotification object:nil];

    id<MTLDevice> device = view.device ?: MTLCreateSystemDefaultDevice();
    if (!device) return nil;
    view.device = device;
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    view.preferredFramesPerSecond = 60;
    view.enableSetNeedsDisplay = NO;
    view.paused = NO;
    view.framebufferOnly = YES;

    _commandQueue = [device newCommandQueue];
    id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLFunction> vertex = [library newFunctionWithName:@"metseVertex"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"metseFragment"];
    if (!_commandQueue || !vertex || !fragment) return nil;
    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = vertex;
    descriptor.fragmentFunction = fragment;
    descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    NSError *error = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!_pipeline) {
        NSLog(@"METSE Metal pipeline failed: %@", error);
        return nil;
    }
    _audioPresenter=[METSEAudioPresenter new];
    view.delegate = self;
    return self;
}

- (void)dealloc {
    [_audioPresenter stop];
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)handleTimingBoundary:(NSNotification *)note {
    (void)note;
    const CFTimeInterval now = CACurrentMediaTime();
    os_unfair_lock_lock(&_telemetryLock);
    _lastFrameTime = now;
    _suppressNextFrameGap = YES;
    ++_lifecycleTimingResets;
    os_unfair_lock_unlock(&_telemetryLock);
}

- (void)start {
    _running = YES;
    os_unfair_lock_lock(&_telemetryLock);
    _lastFrameTime = CACurrentMediaTime();
    _suppressNextFrameGap = YES;
    os_unfair_lock_unlock(&_telemetryLock);
    [_audioPresenter start];
    self.metalView.paused = NO;
}

- (void)stop {
    _running = NO;
    self.metalView.paused = YES;
    os_unfair_lock_lock(&_coreLock);
    _core.setMovementInput(0, 0);
    _core.setSprintHeld(false);
    _core.setAimHeld(false);
    os_unfair_lock_unlock(&_coreLock);
    [_audioPresenter stop];
}

- (void)setMoveForward:(float)forward strafe:(float)strafe { os_unfair_lock_lock(&_coreLock); _core.setMovementInput(forward, strafe); os_unfair_lock_unlock(&_coreLock); }
- (void)addLookYaw:(float)yaw pitch:(float)pitch { os_unfair_lock_lock(&_coreLock); _core.addLookInput(yaw, pitch); os_unfair_lock_unlock(&_coreLock); }
- (void)setSprintHeld:(BOOL)held { os_unfair_lock_lock(&_coreLock); _core.setSprintHeld(held); os_unfair_lock_unlock(&_coreLock); }
- (void)setAimHeld:(BOOL)held { os_unfair_lock_lock(&_coreLock); _core.setAimHeld(held); os_unfair_lock_unlock(&_coreLock); }
- (void)cycleStance { os_unfair_lock_lock(&_coreLock); _core.cycleStance(); os_unfair_lock_unlock(&_coreLock); }
- (void)triggerFire { os_unfair_lock_lock(&_coreLock); _core.triggerFire(); os_unfair_lock_unlock(&_coreLock); }
- (void)reloadWeapon { os_unfair_lock_lock(&_coreLock); _core.reloadWeapon(); os_unfair_lock_unlock(&_coreLock); }

static NSString *METSEStanceName(metse::CharacterStance stance) {
    switch (stance) { case metse::CharacterStance::Standing: return @"STAND"; case metse::CharacterStance::Crouched: return @"CROUCH"; case metse::CharacterStance::Prone: return @"PRONE"; }
    return @"UNKNOWN";
}

static NSString *METSEGaitName(metse::CharacterGait gait) {
    switch (gait) { case metse::CharacterGait::Idle:return @"IDLE";case metse::CharacterGait::Walk:return @"WALK";case metse::CharacterGait::Tactical:return @"TACTICAL";case metse::CharacterGait::Jog:return @"JOG";case metse::CharacterGait::Sprint:return @"SPRINT";case metse::CharacterGait::Crouch:return @"CROUCH";case metse::CharacterGait::Crawl:return @"CRAWL"; }
    return @"UNKNOWN";
}

static NSString *METSECombatStateName(metse::CombatState state) {
    switch (state) { case metse::CombatState::Effective:return @"EFFECTIVE";case metse::CombatState::Wounded:return @"WOUNDED";case metse::CombatState::Incapacitated:return @"INCAPACITATED";case metse::CombatState::Dead:return @"DEAD"; }
    return @"UNKNOWN";
}

static NSString *METSESquadOrderName(metse::AISquadOrder order) {
    switch (order) { case metse::AISquadOrder::Hold:return @"HOLD";case metse::AISquadOrder::Search:return @"SEARCH";case metse::AISquadOrder::Assault:return @"ASSAULT";case metse::AISquadOrder::Defend:return @"DEFEND"; }
    return @"UNKNOWN";
}

- (void)updateThermalPresentationIfNeeded:(CFTimeInterval)now {
    if (now < _nextThermalCheck) return;
    _nextThermalCheck = now + 1.0;
    NSProcessInfoThermalState state = NSProcessInfo.processInfo.thermalState;
    NSInteger target = (state == NSProcessInfoThermalStateSerious || state == NSProcessInfoThermalStateCritical) ? 30 : 60;
    if (_presentationFPS != target) {
        _presentationFPS = target;
        self.metalView.preferredFramesPerSecond = target;
    }
}

- (NSString *)statusString {
    metse::EngineCore coreCopy;
    os_unfair_lock_lock(&_coreLock);
    coreCopy = _core;
    os_unfair_lock_unlock(&_coreLock);
    const auto state = coreCopy.snapshot();
    const auto diagnostics = coreCopy.diagnostics();
    return [NSString stringWithFormat:@"%@/%@ %.1fm/s • %u/%u • ADS %.0f%% • P %u • AI %u • JRN %@ • BB %llu",
            METSEStanceName(state.stance), METSEGaitName(state.gait), state.horizontalSpeed,
            state.ammoInMagazine, state.reserveAmmo, state.adsAlpha * 100.0,
            state.activeProjectiles, state.tacticalAI.engagedAgents,
            diagnostics.journalValid ? @"OK" : @"FAIL", (unsigned long long)diagnostics.retainedBlackBoxFrames];
}

- (NSDictionary<NSString *, id> *)observatorySnapshot {
    // Copy the bounded core under the live lock, then perform percentile sorting and
    // journal verification away from render ownership.
    metse::EngineCore coreCopy;
    os_unfair_lock_lock(&_coreLock);
    coreCopy = _core;
    os_unfair_lock_unlock(&_coreLock);
    const auto s = coreCopy.snapshot();
    const auto d = coreCopy.diagnostics();
    const auto o = d.observatory;
    const auto stateHash = metse::sha256Hex(d.stateHash);
    const auto journalHead = metse::sha256Hex(d.journalHead);

    os_unfair_lock_lock(&_telemetryLock);
    uint64_t rendered = _renderedFrames, misses = _drawableMisses;
    double cpuTotal = _renderCpuTotalMilliseconds, cpuMax = _renderCpuMaxMilliseconds;
    uint64_t lockSamples = _coreLockSamples, callbackSpikes = _callbackGapsOver250ms, lifecycleResets = _lifecycleTimingResets;
    uint64_t cueSnapshotDrops = _audioCueSnapshotDrops;
    double lockWaitTotal = _coreLockWaitTotalMilliseconds, lockWaitMax = _coreLockWaitMaxMilliseconds;
    double coreCriticalTotal = _coreCriticalTotalMilliseconds, coreCriticalMax = _coreCriticalMaxMilliseconds;
    double callbackGapMax = _callbackGapMaxMilliseconds;
    os_unfair_lock_unlock(&_telemetryLock);

    const double cpuAvg = rendered ? cpuTotal / (double)rendered : 0.0;
    const double lockWaitAvg = lockSamples ? lockWaitTotal / (double)lockSamples : 0.0;
    const double coreCriticalAvg = lockSamples ? coreCriticalTotal / (double)lockSamples : 0.0;
    return @{
        @"version":@"0.3.0", @"build":@8, @"presentationFPS":@(_presentationFPS),
        @"simulationTick":@(s.simulationTick), @"simulationSeconds":@(s.simulationSeconds),
        @"stance":METSEStanceName(s.stance), @"gait":METSEGaitName(s.gait), @"speed":@(s.horizontalSpeed),
        @"playerX":@(s.playerX), @"playerY":@(s.playerY), @"playerZ":@(s.playerZ), @"grounded":@(s.grounded),
        @"ammo":@(s.ammoInMagazine), @"reserveAmmo":@(s.reserveAmmo), @"adsAlpha":@(s.adsAlpha), @"reloading":@(s.reloading), @"reloadRemaining":@(s.reloadRemaining), @"weaponObstructed":@(s.weaponObstructed),
        @"shotsFired":@(s.shotsFired), @"activeProjectiles":@(s.activeProjectiles),
        @"damageHits":@(s.damageHits), @"damageIncapacitations":@(s.damageIncapacitations), @"damageKills":@(s.damageKills),
        @"damageArmorHits":@(d.damageArmorHits), @"damageBleedTransitions":@(d.damageBleedTransitions),
        @"primaryTargetHealth":@(s.primaryTargetHealth), @"primaryTargetBleedingPerSecond":@(s.primaryTargetBleedingPerSecond),
        @"primaryTargetHelmetArmorJoules":@(s.primaryTargetHelmetArmorJoules), @"primaryTargetTorsoArmorJoules":@(s.primaryTargetTorsoArmorJoules),
        @"primaryTargetCombatState":METSECombatStateName(s.primaryTargetCombatState),
        @"averageFrameMs":@(o.averageFrameMilliseconds), @"p95FrameMs":@(o.p95FrameMilliseconds), @"p99FrameMs":@(o.p99FrameMilliseconds), @"maxFrameMs":@(o.maxFrameMilliseconds), @"estimatedFPS":@(o.estimatedFPS), @"onePercentLowFPS":@(o.onePercentLowFPS), @"pointOnePercentLowFPS":@(o.pointOnePercentLowFPS),
        @"framesOver20ms":@(o.framesOver20ms), @"framesOver33ms":@(o.framesOver33ms), @"catchUpClampedFrames":@(o.catchUpClampedFrames),
        @"simulationSliceAverageMs":@(o.averageSimulationSliceMilliseconds), @"simulationSliceMaxMs":@(o.maxSimulationSliceMilliseconds), @"simulationSlicesOver20ms":@(o.simulationSlicesOver20ms),
        @"projectileContacts":@(o.totalProjectileContacts), @"projectileTerminalContacts":@(o.totalProjectileTerminalContacts), @"projectileTargetContacts":@(o.totalProjectileTargetContacts),
        @"queueDepth":@(d.inputQueueDepth), @"queueHighWatermark":@(d.inputQueue.highWatermark), @"queueCoalesced":@(d.inputQueue.coalesced), @"queueEvicted":@(d.inputQueue.evictedCoalescible), @"queueRejectedCritical":@(d.inputQueue.rejectedCritical), @"queueRejectedInvalid":@(d.inputQueue.rejectedInvalid),
        @"projectilesSpawned":@(d.ballistics.spawned), @"worldImpacts":@(d.ballistics.worldImpacts), @"terminalWorldImpacts":@(d.ballistics.terminalWorldImpacts), @"targetImpacts":@(d.ballistics.targetImpacts), @"penetrations":@(d.ballistics.penetrations), @"ricochets":@(d.ballistics.ricochets),
        @"audioCues":@(d.audioFX.cuesEmitted), @"audioFootsteps":@(d.audioFX.footsteps), @"audioOutdoorShots":@(d.audioFX.outdoorShots), @"audioIndoorShots":@(d.audioFX.indoorShots), @"audioBulletCracks":@(d.audioFX.bulletCracks), @"audioNearMisses":@(d.audioFX.nearMisses),
        @"fxActive":@(d.audioFX.activeFX), @"fxSpawned":@(d.audioFX.fxSpawned), @"fxDropped":@(d.audioFX.fxDropped),
        @"audioPresentationDrops":@(_audioPresenter.droppedVoiceCount), @"audioCueSnapshotDrops":@(cueSnapshotDrops),
        @"visibilityFull":@(d.visibility.full), @"visibilityReduced":@(d.visibility.reduced), @"visibilityMinimal":@(d.visibility.minimal), @"visibilityDormant":@(d.visibility.dormant), @"visibilityOccluded":@(d.visibility.occluded), @"visibilityBudgetDemotions":@(d.visibility.budgetDemotions),
        @"aiActive":@(d.tacticalAI.activeAgents), @"aiLOS":@(d.tacticalAI.lineOfSightAgents), @"aiHearing":@(d.tacticalAI.hearingAgents), @"aiSuspicious":@(d.tacticalAI.suspiciousAgents), @"aiInvestigating":@(d.tacticalAI.investigatingAgents), @"aiEngaged":@(d.tacticalAI.engagedAgents), @"aiHighestThreat":@(d.tacticalAI.highestThreat), @"aiSquadOrder":METSESquadOrderName(d.tacticalAI.squadOrder),
        @"aiDecisions":@(d.tacticalAI.decisionsExecuted), @"aiPerceptionAgents":@(o.latestAIActiveAgents), @"aiPerceptionLOS":@(o.latestAILOSAgents),
        @"collisionContacts":@(d.sessionCollisionContacts), @"worldObstacleCount":@(d.worldObstacleCount),
        @"journalValid":@(d.journalValid), @"worldValid":@(d.worldValid), @"observatoryValid":@(d.observatoryValid), @"queueValid":@(d.inputQueueValid), @"weaponValid":@(d.weaponValid), @"ballisticsValid":@(d.ballisticsValid), @"damageValid":@(d.damageValid), @"visibilityValid":@(d.visibilityValid), @"tacticalAIValid":@(d.tacticalAIValid), @"audioFXValid":@(d.audioFXValid),
        @"commandsCommitted":@(d.integrity.commandsCommitted), @"commandsRejected":@(d.integrity.commandsRejected), @"commandsRolledBack":@(d.integrity.commandsRolledBack), @"simulationInvariantRollbacks":@(d.simulationInvariantRollbacks), @"blackBoxFrames":@(d.retainedBlackBoxFrames), @"preSpikeBlackBoxFrames":@(d.preSpikeBlackBoxFrames),
        @"denyFireCooldown":@(d.gameplayDenials.fireCooldown), @"denyFireReloading":@(d.gameplayDenials.fireReloading), @"denyFireObstructed":@(d.gameplayDenials.fireObstructed), @"denyFireEmpty":@(d.gameplayDenials.fireEmpty), @"denyFireSprintRecovery":@(d.gameplayDenials.fireSprintRecovery), @"denyProjectileCapacity":@(d.gameplayDenials.projectileCapacity), @"denyReloadInvalid":@(d.gameplayDenials.reloadInvalid), @"autoReloadStarted":@(d.gameplayDenials.autoReloadStarted),
        @"stateHash":[NSString stringWithUTF8String:stateHash.c_str()], @"journalHead":[NSString stringWithUTF8String:journalHead.c_str()],
        @"renderedFrames":@(rendered), @"drawableMisses":@(misses), @"renderCpuAverageMs":@(cpuAvg), @"renderCpuMaxMs":@(cpuMax),
        @"coreLockWaitAverageMs":@(lockWaitAvg), @"coreLockWaitMaxMs":@(lockWaitMax), @"coreCriticalAverageMs":@(coreCriticalAvg), @"coreCriticalMaxMs":@(coreCriticalMax),
        @"callbackGapMaxMs":@(callbackGapMax), @"callbackGapsOver250ms":@(callbackSpikes), @"lifecycleTimingResets":@(lifecycleResets)
    };
}

- (NSString *)observatoryReportText {
    NSDictionary *s = [self observatorySnapshot];
    return [NSString stringWithFormat:
        @"METSE OBSERVATORY V4 / BUILD009-H\nVersion %@ Build %@\nPresentation %@ FPS / Simulation 60 Hz\nTick %@ / %.2fs\nFrame %.1f FPS avg %.2fms p95 %.2f p99 %.2f max %.2f | 1%% low %.1f | 0.1%% low %.1f\nInput Q %@ peak %@ coalesced %@ evicted %@ rejectedCritical %@ rejectedInvalid %@\nWeapon %@/%@ ADS %.0f%% reload %@ obstructed %@\nCombat shots %@ projectiles %@ hits %@ incap %@ kills %@ | targetHP %.1f state %@ bleed %.2f/s\nArmor helmet %.0fJ torso %.0fJ | armorHits %@ bleedTransitions %@\nBallistics spawned %@ worldContacts %@ terminalWorld %@ targetImpacts %@ penetrations %@ ricochets %@\nAI active %@ LOS %@ hearing %@ engaged %@ order %@ threat %.2f\nAudio cues %@ steps %@ shots O/I %@/%@ crack %@ near %@ | FX active %@ spawned %@ dropped %@ | presentationDrop %@ snapshotDrop %@\nVisibility F/R/M/D %@/%@/%@/%@ occluded %@ demoted %@\nIntegrity JRN %@ committed %@ rejected %@ rollback %@ simRollback %@\nGameplayDenials cooldown %@ reloading %@ obstructed %@ empty %@ sprintRecovery %@ capacity %@ reloadInvalid %@ autoReload %@\nRenderer frames %@ misses %@ CPU avg %.3fms max %.3fms\nTiming lockWait avg %.3fms max %.3fms | coreCritical avg %.3fms max %.3fms | callbackGap max %.2fms >250ms %@ lifecycleResets %@\n009-H slice avg %.3fms max %.3fms >20ms %@ | projectile contacts %@ terminal %@ target %@ | AI decisions %@\nBlackBox preSpike %@\nStateHash %@\nJournalHead %@\n",
        s[@"version"],s[@"build"],s[@"presentationFPS"],s[@"simulationTick"],[s[@"simulationSeconds"] doubleValue],
        [s[@"estimatedFPS"] doubleValue],[s[@"averageFrameMs"] doubleValue],[s[@"p95FrameMs"] doubleValue],[s[@"p99FrameMs"] doubleValue],[s[@"maxFrameMs"] doubleValue],[s[@"onePercentLowFPS"] doubleValue],[s[@"pointOnePercentLowFPS"] doubleValue],
        s[@"queueDepth"],s[@"queueHighWatermark"],s[@"queueCoalesced"],s[@"queueEvicted"],s[@"queueRejectedCritical"],s[@"queueRejectedInvalid"],
        s[@"ammo"],s[@"reserveAmmo"],[s[@"adsAlpha"] doubleValue]*100.0,[s[@"reloading"] boolValue]?@"YES":@"NO",[s[@"weaponObstructed"] boolValue]?@"YES":@"NO",
        s[@"shotsFired"],s[@"activeProjectiles"],s[@"damageHits"],s[@"damageIncapacitations"],s[@"damageKills"],[s[@"primaryTargetHealth"] doubleValue],s[@"primaryTargetCombatState"],[s[@"primaryTargetBleedingPerSecond"] doubleValue],
        [s[@"primaryTargetHelmetArmorJoules"] doubleValue],[s[@"primaryTargetTorsoArmorJoules"] doubleValue],s[@"damageArmorHits"],s[@"damageBleedTransitions"],
        s[@"projectilesSpawned"],s[@"worldImpacts"],s[@"terminalWorldImpacts"],s[@"targetImpacts"],s[@"penetrations"],s[@"ricochets"],
        s[@"aiActive"],s[@"aiLOS"],s[@"aiHearing"],s[@"aiEngaged"],s[@"aiSquadOrder"],[s[@"aiHighestThreat"] doubleValue],
        s[@"audioCues"],s[@"audioFootsteps"],s[@"audioOutdoorShots"],s[@"audioIndoorShots"],s[@"audioBulletCracks"],s[@"audioNearMisses"],s[@"fxActive"],s[@"fxSpawned"],s[@"fxDropped"],s[@"audioPresentationDrops"],s[@"audioCueSnapshotDrops"],
        s[@"visibilityFull"],s[@"visibilityReduced"],s[@"visibilityMinimal"],s[@"visibilityDormant"],s[@"visibilityOccluded"],s[@"visibilityBudgetDemotions"],
        [s[@"journalValid"] boolValue]?@"OK":@"FAIL",s[@"commandsCommitted"],s[@"commandsRejected"],s[@"commandsRolledBack"],s[@"simulationInvariantRollbacks"],
        s[@"denyFireCooldown"],s[@"denyFireReloading"],s[@"denyFireObstructed"],s[@"denyFireEmpty"],s[@"denyFireSprintRecovery"],s[@"denyProjectileCapacity"],s[@"denyReloadInvalid"],s[@"autoReloadStarted"],
        s[@"renderedFrames"],s[@"drawableMisses"],[s[@"renderCpuAverageMs"] doubleValue],[s[@"renderCpuMaxMs"] doubleValue],
        [s[@"coreLockWaitAverageMs"] doubleValue],[s[@"coreLockWaitMaxMs"] doubleValue],[s[@"coreCriticalAverageMs"] doubleValue],[s[@"coreCriticalMaxMs"] doubleValue],[s[@"callbackGapMaxMs"] doubleValue],s[@"callbackGapsOver250ms"],s[@"lifecycleTimingResets"],
        [s[@"simulationSliceAverageMs"] doubleValue],[s[@"simulationSliceMaxMs"] doubleValue],s[@"simulationSlicesOver20ms"],s[@"projectileContacts"],s[@"projectileTerminalContacts"],s[@"projectileTargetContacts"],s[@"aiDecisions"],s[@"preSpikeBlackBoxFrames"],s[@"stateHash"],s[@"journalHead"]];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size { (void)view; (void)size; }

- (void)drawInMTKView:(MTKView *)view {
    if (!_running) return;
    CFTimeInterval frameStart = CACurrentMediaTime();
    [self updateThermalPresentationIfNeeded:frameStart];
    double rawDelta = 0.0;
    BOOL suppressGap = NO;
    os_unfair_lock_lock(&_telemetryLock);
    rawDelta = frameStart - _lastFrameTime;
    _lastFrameTime = frameStart;
    suppressGap = _suppressNextFrameGap;
    _suppressNextFrameGap = NO;
    const double rawGapMs = std::max(0.0, rawDelta * 1000.0);
    _callbackGapMaxMilliseconds = std::max(_callbackGapMaxMilliseconds, rawGapMs);
    if (!suppressGap && rawDelta > 0.250) ++_callbackGapsOver250ms;
    os_unfair_lock_unlock(&_telemetryLock);
    const double simulationDelta = suppressGap ? 0.0 : rawDelta;

    metse::EngineSnapshot state{};
    std::array<metse::WorldObstacle, metse::WorldCollisionCore::kMaxObstacles> obstacles{};
    std::array<metse::Projectile, metse::BallisticsCore::kMaxProjectiles> projectiles{};
    std::array<metse::DamageTarget, metse::DamageCore::kMaxTargets> targets{};
    std::array<metse::VisibilityEntity, metse::VisibilityCore::kMaxEntities> visibilityEntities{};
    std::array<metse::FXInstance, metse::AudioFXCore::kFXCapacity> effects{};
    std::array<metse::AudioCue, metse::AudioFXCore::kCueCapacity> pendingAudioCues{};
    std::size_t obstacleCount=0,targetCount=0,visibilityCount=0,pendingAudioCueCount=0;
    uint64_t cueSnapshotDropsThisFrame=0;
    double minX=0,maxX=0,minZ=0,maxZ=0;

    const CFTimeInterval lockRequested = CACurrentMediaTime();
    os_unfair_lock_lock(&_coreLock);
    const CFTimeInterval lockAcquired = CACurrentMediaTime();
    _core.advance(simulationDelta);
    state = _core.snapshot();
    obstacles = _core.worldObstacles();
    obstacleCount = _core.worldObstacleCount();
    const auto &world = _core.worldCollision();
    minX=world.minWorldX(); maxX=world.maxWorldX(); minZ=world.minWorldZ(); maxZ=world.maxWorldZ();
    projectiles = _core.projectiles();
    targets = _core.damageTargets();
    targetCount = _core.damageTargetCount();
    visibilityEntities = _core.visibilityCore().entities();
    visibilityCount = _core.visibilityCore().count();
    effects = _core.audioFX().fxInstances();
    const uint64_t latestCueSequence=_core.audioFX().latestCueSequence();
    if(_lastConsumedAudioCueSequence>latestCueSequence) _lastConsumedAudioCueSequence=0;
    if(latestCueSequence>_lastConsumedAudioCueSequence){
        const uint64_t oldestRetained=latestCueSequence>metse::AudioFXCore::kCueCapacity
            ?latestCueSequence-static_cast<uint64_t>(metse::AudioFXCore::kCueCapacity)+1:1;
        uint64_t firstSequence=_lastConsumedAudioCueSequence+1;
        if(firstSequence<oldestRetained){
            cueSnapshotDropsThisFrame=oldestRetained-firstSequence;
            firstSequence=oldestRetained;
        }
        const uint64_t available=latestCueSequence-firstSequence+1;
        const std::size_t copyCount=static_cast<std::size_t>(
            std::min<uint64_t>(available,metse::AudioFXCore::kCueCapacity));
        for(std::size_t i=0;i<copyCount;++i){
            metse::AudioCue cue{};
            if(_core.audioFX().cueBySequence(firstSequence+static_cast<uint64_t>(i),cue)){
                pendingAudioCues[pendingAudioCueCount++]=cue;
            }
        }
        _lastConsumedAudioCueSequence=latestCueSequence;
    }
    const CFTimeInterval coreFinished = CACurrentMediaTime();
    os_unfair_lock_unlock(&_coreLock);

    const double lockWaitMs = (lockAcquired - lockRequested) * 1000.0;
    const double coreCriticalMs = (coreFinished - lockAcquired) * 1000.0;
    os_unfair_lock_lock(&_telemetryLock);
    ++_coreLockSamples;
    _coreLockWaitTotalMilliseconds += lockWaitMs;
    _coreLockWaitMaxMilliseconds = std::max(_coreLockWaitMaxMilliseconds, lockWaitMs);
    _coreCriticalTotalMilliseconds += coreCriticalMs;
    _coreCriticalMaxMilliseconds = std::max(_coreCriticalMaxMilliseconds, coreCriticalMs);
    _audioCueSnapshotDrops += cueSnapshotDropsThisFrame;
    os_unfair_lock_unlock(&_telemetryLock);

    // Presentation-only spatialization consumes immutable simulation cues. No cue is
    // fed back into gameplay and the native voice pool applies its own fixed drop cap.
    for(std::size_t i=0;i<pendingAudioCueCount;++i){
        const auto& cue=pendingAudioCues[i];
        const double dx=cue.position.x-state.playerX;
        const double dy=cue.position.y-(state.playerY+state.cameraHeight);
        const double dz=cue.position.z-state.playerZ;
        const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        const double horizontal=std::hypot(dx,dz);
        const double rightProjection=horizontal>1e-6
            ?(dx*std::cos(state.playerYaw)-dz*std::sin(state.playerYaw))/horizontal:0.0;
        const float presentationGain=static_cast<float>(
            std::clamp(cue.gain/(1.0+0.045*distance),0.0,1.0));
        [_audioPresenter consumeCueKind:static_cast<uint8_t>(cue.kind)
                               material:static_cast<uint8_t>(cue.material)
                                   gain:presentationGain
                                  pitch:static_cast<float>(cue.pitch)
                                    pan:static_cast<float>(std::clamp(rightProjection,-1.0,1.0))
                               sequence:cue.sequence];
    }

    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline || !self.commandQueue) {
        os_unfair_lock_lock(&_telemetryLock);
        ++_drawableMisses;
        os_unfair_lock_unlock(&_telemetryLock);
        return;
    }
    METSEFrameUniforms uniforms{};
    float muzzleFlash=0.0f;
    NSUInteger effectWrite=0;
    for(const auto& effect:effects){
        if(!effect.active||effect.lifetimeSeconds<=0.0) continue;
        const float remaining=static_cast<float>(
            std::clamp(1.0-effect.ageSeconds/effect.lifetimeSeconds,0.0,1.0));
        if(effect.kind==metse::FXKind::MuzzleFlash){
            muzzleFlash=std::max(muzzleFlash,remaining*static_cast<float>(effect.intensity));
            continue;
        }
        if(effectWrite>=kRenderFXCap) continue;
        uniforms.fxData[effectWrite]=(vector_float4){(float)effect.position.x,(float)effect.position.y,
                                                     (float)effect.position.z,remaining};
        uniforms.fxMeta[effectWrite]=(vector_float4){(float)static_cast<uint8_t>(effect.kind),
                                                     (float)static_cast<uint8_t>(effect.material),
                                                     (float)effect.intensity,0.0f};
        ++effectWrite;
    }
    uniforms.timing=(vector_float4){(float)state.simulationSeconds,(float)view.drawableSize.width,(float)view.drawableSize.height,muzzleFlash};
    uniforms.camera=(vector_float4){(float)state.playerX,(float)state.playerZ,(float)state.playerYaw,(float)state.playerPitch};
    uniforms.character=(vector_float4){(float)state.cameraHeight,(float)state.playerY,(float)state.horizontalSpeed,(float)state.cameraRoll};
    uniforms.weapon=(vector_float4){(float)state.adsAlpha,(float)state.recoilPitch,(float)state.recoilYaw,state.weaponObstructed?1.0f:0.0f};
    uniforms.weapon2=(vector_float4){(float)state.weaponSwayX,(float)state.weaponSwayY,(float)state.ammoInMagazine,(float)state.reloadRemaining};
    uniforms.worldMeta=(vector_float4){(float)obstacleCount,(float)minX,(float)maxX,(float)minZ};
    const std::size_t renderTargetCount=std::min({targetCount,visibilityCount,
                                                  static_cast<std::size_t>(kRenderTargetCap)});
    uniforms.worldExtra=(vector_float4){(float)maxZ,(float)renderTargetCount,0.0f,(float)state.cameraLean};
    uniforms.presentationMeta=(vector_float4){(float)effectWrite,0.0f,0.0f,0.0f};
    for (NSUInteger i=0;i<kRenderObstacleCap;++i) {
        const auto& obstacle=obstacles[i];
        uniforms.obstacleBounds[i]=(vector_float4){(float)obstacle.minX,(float)obstacle.minZ,(float)obstacle.maxX,(float)obstacle.maxZ};
        uniforms.obstacleMeta[i]=(vector_float4){(float)obstacle.minY,(float)obstacle.maxY,(float)static_cast<std::uint8_t>(obstacle.material),0};
    }
    NSUInteger projectileWrite=0;
    for (const auto& projectile:projectiles) {
        if(!projectile.active || projectileWrite>=kRenderProjectileCap) continue;
        uniforms.projectilePositions[projectileWrite++]=(vector_float4){(float)projectile.position.x,(float)projectile.position.y,(float)projectile.position.z,1};
    }
    uniforms.worldExtra.z=(float)projectileWrite;
    for (NSUInteger i=0;i<renderTargetCount;++i) {
        const auto& target=targets[i];
        const auto& visibility=visibilityEntities[i];
        const bool identityMatches=target.id==visibility.id;
        uniforms.targetData[i]=(vector_float4){(float)target.position.x,(float)target.position.z,(float)target.health,target.alive?1.0f:0.0f};
        uniforms.targetMeta[i]=(vector_float4){identityMatches?(float)static_cast<uint8_t>(visibility.tier):3.0f,
                                              identityMatches&&visibility.lineOfSight?1.0f:0.0f,0.0f,0.0f};
    }

    id<MTLCommandBuffer> commandBuffer=[self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> encoder=[commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:self.pipeline];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    const double cpuMs=(CACurrentMediaTime()-frameStart)*1000.0;
    os_unfair_lock_lock(&_telemetryLock);
    ++_renderedFrames;
    _renderCpuTotalMilliseconds+=cpuMs;
    _renderCpuMaxMilliseconds=std::max(_renderCpuMaxMilliseconds,cpuMs);
    os_unfair_lock_unlock(&_telemetryLock);
}
@end
