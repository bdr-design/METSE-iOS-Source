#import "METSEEngineBridge.h"
#import "METSEAudioPresenter.h"
#import "METSEBallisticFXRenderer.h"
#import "METSEBattlefieldRenderer.h"
#import "METSECombatantRenderer.h"
#import "METSEViewmodelRenderer.h"
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
static constexpr NSUInteger kNativeBallisticFXCap = metse::BallisticsCore::kMaxProjectiles + metse::AudioFXCore::kFXCapacity;
static_assert(kRenderFXCap <= metse::AudioFXCore::kFXCapacity,
              "Presentation FX budget cannot exceed the simulation-owned pool");

static constexpr uint64_t kDiagnosticIntegrity = 1ull << 0;
static constexpr uint64_t kDiagnosticTelemetry = 1ull << 1;
static constexpr uint64_t kDiagnosticCallback = 1ull << 2;
static constexpr uint64_t kDiagnosticCatchUp = 1ull << 3;
static constexpr uint64_t kDiagnosticSimulationSlice = 1ull << 4;
static constexpr uint64_t kDiagnosticInputPressure = 1ull << 5;
static constexpr uint64_t kDiagnosticPresentationDrop = 1ull << 6;
static constexpr uint64_t kDiagnosticFXDrop = 1ull << 7;
static constexpr uint64_t kDiagnosticDrawableMiss = 1ull << 8;
static constexpr uint64_t kDiagnosticThermalCritical = 1ull << 9;
static constexpr uint64_t kDiagnosticMemoryPressure = 1ull << 10;
static constexpr uint64_t kCoverageShortRun = 1ull << 0;
static constexpr uint64_t kCoverageNoAI = 1ull << 1;
static constexpr uint64_t kCoverageBelow32AI = 1ull << 2;
static constexpr uint64_t kCoverageNoProjectile = 1ull << 3;

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
@property(nonatomic, strong) id<MTLRenderPipelineState> overlayPipeline;
@property(nonatomic, strong) METSEBallisticFXRenderer *ballisticFXRenderer;
@property(nonatomic, strong) METSEBattlefieldRenderer *battlefieldRenderer;
@property(nonatomic, strong) METSECombatantRenderer *combatantRenderer;
@property(nonatomic, strong) METSEViewmodelRenderer *viewmodelRenderer;
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
    double _callbackGapTotalMilliseconds;
    uint64_t _callbackSamples;
    uint64_t _callbackGapsOverBudget;
    uint64_t _callbackGapsOver50ms;
    uint64_t _callbackGapsOver100ms;
    uint64_t _callbackGapsOver250ms;
    uint64_t _lifecycleTimingResets;
    uint64_t _lifecycleWillResignActive;
    uint64_t _lifecycleDidEnterBackground;
    uint64_t _lifecycleWillEnterForeground;
    uint64_t _lifecycleDidBecomeActive;
    uint64_t _memoryWarningEvents;
    uint64_t _thermalFallbackFrames;
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
    [center addObserver:self selector:@selector(handleMemoryWarning:) name:UIApplicationDidReceiveMemoryWarningNotification object:nil];

    id<MTLDevice> device = view.device ?: MTLCreateSystemDefaultDevice();
    if (!device) return nil;
    view.device = device;
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
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
    descriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    NSError *error = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!_pipeline) {
        NSLog(@"METSE Metal pipeline failed: %@", error);
        return nil;
    }
    MTLRenderPipelineDescriptor *overlayDescriptor = [MTLRenderPipelineDescriptor new];
    overlayDescriptor.label = @"METSE reticle and muzzle overlay";
    overlayDescriptor.vertexFunction = vertex;
    overlayDescriptor.fragmentFunction = [library newFunctionWithName:@"metseReticleFragment"];
    overlayDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    overlayDescriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    overlayDescriptor.colorAttachments[0].blendingEnabled = YES;
    overlayDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    overlayDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    overlayDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    overlayDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    _overlayPipeline = [device newRenderPipelineStateWithDescriptor:overlayDescriptor error:&error];
    if (!_overlayPipeline) {
        NSLog(@"METSE overlay pipeline failed: %@", error);
        return nil;
    }
    _viewmodelRenderer = [[METSEViewmodelRenderer alloc]
        initWithDevice:device
        library:library
        colorPixelFormat:view.colorPixelFormat
        depthPixelFormat:view.depthStencilPixelFormat
        bundle:NSBundle.mainBundle];
    _battlefieldRenderer = [[METSEBattlefieldRenderer alloc]
        initWithDevice:device
        library:library
        colorPixelFormat:view.colorPixelFormat
        depthPixelFormat:view.depthStencilPixelFormat];
    _combatantRenderer = [[METSECombatantRenderer alloc]
        initWithDevice:device
        library:library
        colorPixelFormat:view.colorPixelFormat
        depthPixelFormat:view.depthStencilPixelFormat];
    _ballisticFXRenderer = [[METSEBallisticFXRenderer alloc]
        initWithDevice:device
        library:library
        colorPixelFormat:view.colorPixelFormat
        depthPixelFormat:view.depthStencilPixelFormat];
    NSLog(@"METSE viewmodel: %@", _viewmodelRenderer.assetStatus);
    NSLog(@"METSE battlefield: %@", _battlefieldRenderer.status);
    NSLog(@"METSE combatants: %@", _combatantRenderer.status);
    NSLog(@"METSE ballistic FX: %@", _ballisticFXRenderer.status);
    _audioPresenter=[METSEAudioPresenter new];
    view.delegate = self;
    return self;
}

- (void)dealloc {
    [_audioPresenter stop];
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)handleTimingBoundary:(NSNotification *)note {
    const CFTimeInterval now = CACurrentMediaTime();
    os_unfair_lock_lock(&_telemetryLock);
    _lastFrameTime = now;
    _suppressNextFrameGap = YES;
    ++_lifecycleTimingResets;
    if ([note.name isEqualToString:UIApplicationWillResignActiveNotification]) {
        ++_lifecycleWillResignActive;
    } else if ([note.name isEqualToString:UIApplicationDidEnterBackgroundNotification]) {
        ++_lifecycleDidEnterBackground;
    } else if ([note.name isEqualToString:UIApplicationWillEnterForegroundNotification]) {
        ++_lifecycleWillEnterForeground;
    } else if ([note.name isEqualToString:UIApplicationDidBecomeActiveNotification]) {
        ++_lifecycleDidBecomeActive;
    }
    os_unfair_lock_unlock(&_telemetryLock);
}

- (void)handleMemoryWarning:(NSNotification *)note {
    (void)note;
    os_unfair_lock_lock(&_telemetryLock);
    ++_memoryWarningEvents;
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

static NSString *METSEThermalStateName(NSProcessInfoThermalState state) {
    switch(state){
        case NSProcessInfoThermalStateNominal:return @"NOMINAL";
        case NSProcessInfoThermalStateFair:return @"FAIR";
        case NSProcessInfoThermalStateSerious:return @"SERIOUS";
        case NSProcessInfoThermalStateCritical:return @"CRITICAL";
    }
    return @"UNKNOWN";
}

- (void)updateThermalPresentationIfNeeded:(CFTimeInterval)now {
    if (now < _nextThermalCheck) return;
    _nextThermalCheck = now + 1.0;
    NSProcessInfoThermalState state = NSProcessInfo.processInfo.thermalState;
    NSInteger target = (state == NSProcessInfoThermalStateSerious || state == NSProcessInfoThermalStateCritical) ? 30 : 60;
    BOOL changed = NO;
    os_unfair_lock_lock(&_telemetryLock);
    if (_presentationFPS != target) {
        _presentationFPS = target;
        changed = YES;
    }
    os_unfair_lock_unlock(&_telemetryLock);
    if (changed) {
        self.metalView.preferredFramesPerSecond = target;
    }
}

- (NSString *)statusString {
    os_unfair_lock_lock(&_coreLock);
    const auto state = _core.snapshot();
    os_unfair_lock_unlock(&_coreLock);
    return [NSString stringWithFormat:@"%@/%@ %.1fm/s • %u/%u • ADS %.0f%% • P %u • AI %u",
            METSEStanceName(state.stance), METSEGaitName(state.gait), state.horizontalSpeed,
            state.ammoInMagazine, state.reserveAmmo, state.adsAlpha * 100.0,
            state.activeProjectiles, state.tacticalAI.engagedAgents];
}

- (NSString *)stanceName {
    os_unfair_lock_lock(&_coreLock);
    const auto stance = _core.snapshot().stance;
    os_unfair_lock_unlock(&_coreLock);
    return METSEStanceName(stance);
}

- (NSDictionary<NSString *, id> *)combatHUDSnapshot {
    os_unfair_lock_lock(&_coreLock);
    const auto state = _core.snapshot();
    os_unfair_lock_unlock(&_coreLock);
    os_unfair_lock_lock(&_telemetryLock);
    const NSInteger presentationFPS = _presentationFPS;
    os_unfair_lock_unlock(&_telemetryLock);
    return @{
        @"ammo": @(state.ammoInMagazine),
        @"reserveAmmo": @(state.reserveAmmo),
        @"health": @(state.playerHealth),
        @"stance": METSEStanceName(state.stance),
        @"reloading": @(state.reloading),
        @"reloadRemaining": @(state.reloadRemaining),
        @"obstructed": @(state.weaponObstructed),
        @"engagedAI": @(state.tacticalAI.engagedAgents),
        @"presentationFPS": @(presentationFPS)
    };
}

- (NSDictionary<NSString *, id> *)observatorySnapshot {
    // Capture only diagnostics inputs; expensive verification stays outside ownership.
    metse::EngineDiagnosticsCapture capture;
    const double captureStart = CACurrentMediaTime();
    os_unfair_lock_lock(&_coreLock);
    const double captureAcquired = CACurrentMediaTime();
    _core.captureDiagnostics(capture);
    os_unfair_lock_unlock(&_coreLock);
    const double captureEnd = CACurrentMediaTime();
    const auto s = capture.snapshot;
    const auto d = capture.finish();
    const double finishEnd = CACurrentMediaTime();
    const auto o = d.observatory;
    const auto stateHash = metse::sha256Hex(d.stateHash);
    const auto journalHead = metse::sha256Hex(d.journalHead);

    os_unfair_lock_lock(&_telemetryLock);
    uint64_t rendered = _renderedFrames, misses = _drawableMisses;
    double cpuTotal = _renderCpuTotalMilliseconds, cpuMax = _renderCpuMaxMilliseconds;
    uint64_t lockSamples = _coreLockSamples, callbackSamples = _callbackSamples;
    uint64_t callbackBudgetSpikes = _callbackGapsOverBudget, callback50Spikes = _callbackGapsOver50ms;
    uint64_t callback100Spikes = _callbackGapsOver100ms, callbackSpikes = _callbackGapsOver250ms;
    uint64_t lifecycleResets = _lifecycleTimingResets, thermalFallbackFrames = _thermalFallbackFrames;
    uint64_t lifecycleWillResignActive = _lifecycleWillResignActive;
    uint64_t lifecycleDidEnterBackground = _lifecycleDidEnterBackground;
    uint64_t lifecycleWillEnterForeground = _lifecycleWillEnterForeground;
    uint64_t lifecycleDidBecomeActive = _lifecycleDidBecomeActive;
    uint64_t memoryWarnings = _memoryWarningEvents;
    uint64_t cueSnapshotDrops = _audioCueSnapshotDrops;
    double lockWaitTotal = _coreLockWaitTotalMilliseconds, lockWaitMax = _coreLockWaitMaxMilliseconds;
    double coreCriticalTotal = _coreCriticalTotalMilliseconds, coreCriticalMax = _coreCriticalMaxMilliseconds;
    double callbackGapTotal = _callbackGapTotalMilliseconds, callbackGapMax = _callbackGapMaxMilliseconds;
    NSInteger presentationFPS = _presentationFPS;
    os_unfair_lock_unlock(&_telemetryLock);

    const double cpuAvg = rendered ? cpuTotal / (double)rendered : 0.0;
    const double lockWaitAvg = lockSamples ? lockWaitTotal / (double)lockSamples : 0.0;
    const double coreCriticalAvg = lockSamples ? coreCriticalTotal / (double)lockSamples : 0.0;
    const double callbackGapAvg = callbackSamples ? callbackGapTotal / (double)callbackSamples : 0.0;
    const NSProcessInfoThermalState thermalState = NSProcessInfo.processInfo.thermalState;
    uint64_t diagnosticProblemMask = 0;
    if(thermalState==NSProcessInfoThermalStateCritical) diagnosticProblemMask |= kDiagnosticThermalCritical;
    if(!d.journalValid || !d.worldValid || !d.observatoryValid || !d.inputQueueValid || !d.weaponValid ||
       !d.ballisticsValid || !d.damageValid || !d.visibilityValid || !d.tacticalAIValid || !d.audioFXValid){
        diagnosticProblemMask |= kDiagnosticIntegrity;
    }
    if(o.rejectedSamples>0 || o.simulationTickRegressions>0) diagnosticProblemMask |= kDiagnosticTelemetry;
    if(d.preSpikeCallbackFrames>0 || callbackBudgetSpikes>0 || callbackSpikes>0 ||
       (presentationFPS>=60 && o.windowFramesOver20ms>0)) diagnosticProblemMask |= kDiagnosticCallback;
    if(d.preSpikeCatchUpFrames>0 || o.catchUpClampedFrames>0) diagnosticProblemMask |= kDiagnosticCatchUp;
    if(d.preSpikeSimulationFrames>0 || o.simulationSlicesOver20ms>0) diagnosticProblemMask |= kDiagnosticSimulationSlice;
    if(d.inputQueue.highWatermark>=metse::InputCommandQueue::kCapacity || d.inputQueue.evictedCoalescible>0 ||
       d.inputQueue.rejectedCritical>0 || d.inputQueue.rejectedInvalid>0) diagnosticProblemMask |= kDiagnosticInputPressure;
    if(_audioPresenter.droppedVoiceCount>0 || cueSnapshotDrops>0) diagnosticProblemMask |= kDiagnosticPresentationDrop;
    if(d.audioFX.fxDropped>0) diagnosticProblemMask |= kDiagnosticFXDrop;
    if(misses>0) diagnosticProblemMask |= kDiagnosticDrawableMiss;
    if(memoryWarnings>0) diagnosticProblemMask |= kDiagnosticMemoryPressure;
    uint64_t coverageMask = 0;
    if(o.observedRealSeconds<300.0) coverageMask |= kCoverageShortRun;
    if(o.peakAIActiveAgents==0) coverageMask |= kCoverageNoAI;
    if(o.fullCombatantLoadSeconds<=0.0) coverageMask |= kCoverageBelow32AI;
    if(d.ballistics.spawned==0) coverageMask |= kCoverageNoProjectile;
    return @{
        @"diagnosticCaptureWaitMs":@((captureAcquired-captureStart)*1000.0),
        @"diagnosticCaptureMs":@((captureEnd-captureAcquired)*1000.0),
        @"diagnosticFinishMs":@((finishEnd-captureEnd)*1000.0),
        @"diagnosticCaptureBytes":@(sizeof(metse::EngineDiagnosticsCapture)),
        @"engineCoreBytes":@(sizeof(metse::EngineCore)),
        @"fullCombatantLoadSeconds":@(o.fullCombatantLoadSeconds),
        @"combinedLoadSeconds":@(o.combinedLoadSeconds),
        @"sourceCommit":NSBundle.mainBundle.infoDictionary[@"METSESourceCommit"] ?: @"UNSTAMPED",
        @"version":NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"UNKNOWN",
        @"build":NSBundle.mainBundle.infoDictionary[@"CFBundleVersion"] ?: @"UNKNOWN",
        @"presentationFPS":@(presentationFPS),
        @"thermalState":METSEThermalStateName(thermalState), @"thermalFallbackActive":@(presentationFPS==30),
        @"thermalFallbackFrames":@(thermalFallbackFrames),
        @"simulationTick":@(s.simulationTick), @"simulationSeconds":@(s.simulationSeconds),
        @"stance":METSEStanceName(s.stance), @"gait":METSEGaitName(s.gait), @"speed":@(s.horizontalSpeed),
        @"playerX":@(s.playerX), @"playerY":@(s.playerY), @"playerZ":@(s.playerZ), @"grounded":@(s.grounded),
        @"ammo":@(s.ammoInMagazine), @"reserveAmmo":@(s.reserveAmmo), @"adsAlpha":@(s.adsAlpha), @"reloading":@(s.reloading), @"reloadRemaining":@(s.reloadRemaining), @"weaponObstructed":@(s.weaponObstructed),
        @"shotsFired":@(s.shotsFired), @"activeProjectiles":@(s.activeProjectiles),
        @"damageHits":@(s.damageHits), @"damageIncapacitations":@(s.damageIncapacitations), @"damageKills":@(s.damageKills),
        @"playerHealth":@(s.playerHealth), @"playerBleedingPerSecond":@(s.playerBleedingPerSecond),
        @"playerCombatState":METSECombatStateName(s.playerCombatState),
        @"aiShotsFired":@(s.aiShotsFired), @"aiTargetImpacts":@(s.aiTargetImpacts),
        @"friendlyFireDenials":@(s.friendlyFireDenials), @"combatantCount":@(s.combatantCount),
        @"damageArmorHits":@(d.damageArmorHits), @"damageBleedTransitions":@(d.damageBleedTransitions),
        @"primaryTargetHealth":@(s.primaryTargetHealth), @"primaryTargetBleedingPerSecond":@(s.primaryTargetBleedingPerSecond),
        @"primaryTargetHelmetArmorJoules":@(s.primaryTargetHelmetArmorJoules), @"primaryTargetTorsoArmorJoules":@(s.primaryTargetTorsoArmorJoules),
        @"primaryTargetCombatState":METSECombatStateName(s.primaryTargetCombatState),
        @"observedFrames":@(o.observedFrames), @"retainedFrames":@(o.retainedFrames), @"observedRealSeconds":@(o.observedRealSeconds), @"retainedRealSeconds":@(o.retainedRealSeconds), @"retainedSimulationTicks":@(o.retainedSimulationTicks),
        @"telemetryRejectedSamples":@(o.rejectedSamples), @"telemetryRejectedNonFinite":@(o.rejectedNonFiniteSamples), @"telemetryRejectedRange":@(o.rejectedRangeSamples), @"simulationTickRegressions":@(o.simulationTickRegressions),
        @"sessionAverageFrameMs":@(o.sessionAverageFrameMilliseconds), @"averageFrameMs":@(o.averageFrameMilliseconds), @"p95FrameMs":@(o.p95FrameMilliseconds), @"p99FrameMs":@(o.p99FrameMilliseconds), @"maxFrameMs":@(o.maxFrameMilliseconds), @"estimatedFPS":@(o.estimatedFPS), @"onePercentLowFPS":@(o.onePercentLowFPS), @"pointOnePercentLowFPS":@(o.pointOnePercentLowFPS),
        @"framesOver20ms":@(o.framesOver20ms), @"framesOver33ms":@(o.framesOver33ms), @"windowFramesOver20ms":@(o.windowFramesOver20ms), @"windowFramesOver33ms":@(o.windowFramesOver33ms), @"catchUpClampedFrames":@(o.catchUpClampedFrames), @"windowCatchUpClampedFrames":@(o.windowCatchUpClampedFrames),
        @"sessionSimulationSliceAverageMs":@(o.sessionAverageSimulationSliceMilliseconds), @"simulationSliceAverageMs":@(o.averageSimulationSliceMilliseconds), @"simulationSliceMaxMs":@(o.maxSimulationSliceMilliseconds), @"simulationSlicesOver20ms":@(o.simulationSlicesOver20ms), @"windowSimulationSlicesOver20ms":@(o.windowSimulationSlicesOver20ms),
        @"projectileContacts":@(o.totalProjectileContacts), @"projectileTerminalContacts":@(o.totalProjectileTerminalContacts), @"projectileTargetContacts":@(o.totalProjectileTargetContacts),
        @"queueDepth":@(d.inputQueueDepth), @"queueHighWatermark":@(d.inputQueue.highWatermark), @"queueCoalesced":@(d.inputQueue.coalesced), @"queueEvicted":@(d.inputQueue.evictedCoalescible), @"queueRejectedCritical":@(d.inputQueue.rejectedCritical), @"queueRejectedInvalid":@(d.inputQueue.rejectedInvalid), @"peakProjectiles":@(o.peakProjectiles),
        @"projectilesSpawned":@(d.ballistics.spawned), @"worldImpacts":@(d.ballistics.worldImpacts), @"terminalWorldImpacts":@(d.ballistics.terminalWorldImpacts), @"targetImpacts":@(d.ballistics.targetImpacts), @"penetrations":@(d.ballistics.penetrations), @"ricochets":@(d.ballistics.ricochets),
        @"audioCues":@(d.audioFX.cuesEmitted), @"audioFootsteps":@(d.audioFX.footsteps), @"audioOutdoorShots":@(d.audioFX.outdoorShots), @"audioIndoorShots":@(d.audioFX.indoorShots), @"audioBulletCracks":@(d.audioFX.bulletCracks), @"audioNearMisses":@(d.audioFX.nearMisses),
        @"fxActive":@(d.audioFX.activeFX), @"fxSpawned":@(d.audioFX.fxSpawned), @"fxDropped":@(d.audioFX.fxDropped),
        @"audioPresentationDrops":@(_audioPresenter.droppedVoiceCount), @"audioCueSnapshotDrops":@(cueSnapshotDrops),
        @"visibilityFull":@(d.visibility.full), @"visibilityReduced":@(d.visibility.reduced), @"visibilityMinimal":@(d.visibility.minimal), @"visibilityDormant":@(d.visibility.dormant), @"visibilityOccluded":@(d.visibility.occluded), @"visibilityBudgetDemotions":@(d.visibility.budgetDemotions), @"visibilityPeakFull":@(o.peakVisibilityFull), @"visibilityPeakReduced":@(o.peakVisibilityReduced), @"visibilityPeakMinimal":@(o.peakVisibilityMinimal), @"visibilityPeakDormant":@(o.peakVisibilityDormant),
        @"aiActive":@(d.tacticalAI.activeAgents), @"aiLOS":@(d.tacticalAI.lineOfSightAgents), @"aiHearing":@(d.tacticalAI.hearingAgents), @"aiSuspicious":@(d.tacticalAI.suspiciousAgents), @"aiInvestigating":@(d.tacticalAI.investigatingAgents), @"aiEngaged":@(d.tacticalAI.engagedAgents), @"aiHighestThreat":@(d.tacticalAI.highestThreat), @"aiSquadOrder":METSESquadOrderName(d.tacticalAI.squadOrder),
        @"aiDecisions":@(d.tacticalAI.decisionsExecuted), @"aiPerceptionAgents":@(o.latestAIActiveAgents), @"aiPerceptionLOS":@(o.latestAILOSAgents), @"aiPeakActive":@(o.peakAIActiveAgents), @"aiPeakLOS":@(o.peakAILOSAgents),
        @"aiSuppressed":@(d.tacticalAI.suppressedAgents), @"aiSuppressionChecks":@(d.tacticalAI.suppressionChecksThisStep),
        @"aiSuppressionCheckCap":@(metse::TacticalAICore::kMaxSuppressionChecksPerStep),
        @"aiSuppressionObservations":@(d.tacticalAI.suppressionObservations), @"aiSuppressionBudgetDrops":@(d.tacticalAI.suppressionBudgetDrops),
        @"aiRecovering":@(d.tacticalAI.recoveringAgents), @"aiConcerned":@(d.tacticalAI.concernedAgents),
        @"aiInjuryReactions":@(d.tacticalAI.injuryReactions), @"aiWitnessedLosses":@(d.tacticalAI.witnessedLosses),
        @"aiLossChecks":@(d.tacticalAI.lossChecksThisStep), @"aiLossCheckCap":@(metse::TacticalAICore::kMaxLossChecksPerStep),
        @"aiLossBudgetDrops":@(d.tacticalAI.lossBudgetDrops), @"aiObservedDamageSequence":@(d.tacticalAI.lastObservedDamageSequence),
        @"aiShotsFiredDiagnostics":@(d.aiShotsFired), @"aiTargetImpactsDiagnostics":@(d.aiTargetImpacts), @"friendlyFireDenialsDiagnostics":@(d.friendlyFireDenials), @"combatantRecords":@(d.combatants.count()), @"combatantActive":@(d.combatantLifecycle.active), @"combatantWounded":@(d.combatantLifecycle.wounded), @"combatantIncapacitated":@(d.combatantLifecycle.incapacitated), @"combatantDead":@(d.combatantLifecycle.dead), @"combatantRemoved":@(d.combatantLifecycle.removed),
        @"collisionContacts":@(d.sessionCollisionContacts), @"worldObstacleCount":@(d.worldObstacleCount),
        @"journalValid":@(d.journalValid), @"worldValid":@(d.worldValid), @"observatoryValid":@(d.observatoryValid), @"queueValid":@(d.inputQueueValid), @"weaponValid":@(d.weaponValid), @"ballisticsValid":@(d.ballisticsValid), @"damageValid":@(d.damageValid), @"visibilityValid":@(d.visibilityValid), @"tacticalAIValid":@(d.tacticalAIValid), @"audioFXValid":@(d.audioFXValid),
        @"commandsCommitted":@(d.integrity.commandsCommitted), @"commandsRejected":@(d.integrity.commandsRejected), @"commandsRolledBack":@(d.integrity.commandsRolledBack), @"simulationInvariantRollbacks":@(d.simulationInvariantRollbacks), @"blackBoxFrames":@(d.retainedBlackBoxFrames), @"preSpikeBlackBoxFrames":@(d.preSpikeBlackBoxFrames), @"preSpikeCallbackFrames":@(d.preSpikeCallbackFrames), @"preSpikeCatchUpFrames":@(d.preSpikeCatchUpFrames), @"preSpikeSimulationFrames":@(d.preSpikeSimulationFrames), @"retainedPreSpikeCallbackFrames":@(d.retainedPreSpikeCallbackFrames), @"retainedPreSpikeCatchUpFrames":@(d.retainedPreSpikeCatchUpFrames), @"retainedPreSpikeSimulationFrames":@(d.retainedPreSpikeSimulationFrames),
        @"denyFireCooldown":@(d.gameplayDenials.fireCooldown), @"denyFireReloading":@(d.gameplayDenials.fireReloading), @"denyFireObstructed":@(d.gameplayDenials.fireObstructed), @"denyFireEmpty":@(d.gameplayDenials.fireEmpty), @"denyFireSprintRecovery":@(d.gameplayDenials.fireSprintRecovery), @"denyProjectileCapacity":@(d.gameplayDenials.projectileCapacity), @"denyReloadInvalid":@(d.gameplayDenials.reloadInvalid), @"denyFireCombatDisabled":@(d.gameplayDenials.fireCombatDisabled), @"autoReloadStarted":@(d.gameplayDenials.autoReloadStarted),
        @"stateHash":[NSString stringWithUTF8String:stateHash.c_str()], @"journalHead":[NSString stringWithUTF8String:journalHead.c_str()],
        @"renderedFrames":@(rendered), @"drawableMisses":@(misses), @"renderCpuAverageMs":@(cpuAvg), @"renderCpuMaxMs":@(cpuMax),
        @"coreLockWaitAverageMs":@(lockWaitAvg), @"coreLockWaitMaxMs":@(lockWaitMax), @"coreCriticalAverageMs":@(coreCriticalAvg), @"coreCriticalMaxMs":@(coreCriticalMax),
        @"callbackGapAverageMs":@(callbackGapAvg), @"callbackGapMaxMs":@(callbackGapMax), @"callbackGapsOverBudget":@(callbackBudgetSpikes), @"callbackGapsOver50ms":@(callback50Spikes), @"callbackGapsOver100ms":@(callback100Spikes), @"callbackGapsOver250ms":@(callbackSpikes), @"lifecycleTimingResets":@(lifecycleResets),
        @"lifecycleWillResignActive":@(lifecycleWillResignActive), @"lifecycleDidEnterBackground":@(lifecycleDidEnterBackground), @"lifecycleWillEnterForeground":@(lifecycleWillEnterForeground), @"lifecycleDidBecomeActive":@(lifecycleDidBecomeActive), @"memoryWarningEvents":@(memoryWarnings),
        @"diagnosticProblemMask":@(diagnosticProblemMask), @"acceptanceCoverageMask":@(coverageMask)
    };
}

- (NSString *)observatoryReportText {
    NSDictionary *s = [self observatorySnapshot];
    NSMutableString *report=[NSMutableString stringWithFormat:@"METSE OBSERVATORY V4 / BUILD009-H\nVersion %@ Build %@\nPresentation %@ FPS / Simulation 60 Hz\nThermal %@ fallback %@\nTick %@ / %.2fs\n",
        s[@"version"],s[@"build"],s[@"presentationFPS"],s[@"thermalState"],[s[@"thermalFallbackActive"] boolValue]?@"ACTIVE":@"OFF",s[@"simulationTick"],[s[@"simulationSeconds"] doubleValue]];
    [report appendFormat:@"Window observed %.2fs retained %.2fs (%@/%@ frames)\nTelemetry rejected %@ (nonFinite %@ range %@ tickRegression %@)\n",
        [s[@"observedRealSeconds"] doubleValue],[s[@"retainedRealSeconds"] doubleValue],s[@"retainedFrames"],s[@"observedFrames"],s[@"telemetryRejectedSamples"],s[@"telemetryRejectedNonFinite"],s[@"telemetryRejectedRange"],s[@"simulationTickRegressions"]];
    [report appendFormat:@"Frame window %.1f FPS avg %.2fms p95 %.2f p99 %.2f max %.2f | 1%% low %.1f | 0.1%% low %.1f\nFrame session avg %.2fms >20ms %@ >33ms %@\n",
        [s[@"estimatedFPS"] doubleValue],[s[@"averageFrameMs"] doubleValue],[s[@"p95FrameMs"] doubleValue],[s[@"p99FrameMs"] doubleValue],[s[@"maxFrameMs"] doubleValue],[s[@"onePercentLowFPS"] doubleValue],[s[@"pointOnePercentLowFPS"] doubleValue],
        [s[@"sessionAverageFrameMs"] doubleValue],s[@"framesOver20ms"],s[@"framesOver33ms"]];
    [report appendFormat:@"Input Q %@ peak %@ coalesced %@ evicted %@ rejectedCritical %@ rejectedInvalid %@\n",
        s[@"queueDepth"],s[@"queueHighWatermark"],s[@"queueCoalesced"],s[@"queueEvicted"],s[@"queueRejectedCritical"],s[@"queueRejectedInvalid"]];
    [report appendFormat:@"Weapon %@/%@ ADS %.0f%% reload %@ obstructed %@\n",
        s[@"ammo"],s[@"reserveAmmo"],[s[@"adsAlpha"] doubleValue]*100.0,[s[@"reloading"] boolValue]?@"YES":@"NO",[s[@"weaponObstructed"] boolValue]?@"YES":@"NO"];
    [report appendFormat:@"Combat shots %@ projectiles %@ hits %@ incap %@ kills %@ | targetHP %.1f state %@ bleed %.2f/s\nArmor helmet %.0fJ torso %.0fJ | armorHits %@ bleedTransitions %@\n",
        s[@"shotsFired"],s[@"activeProjectiles"],s[@"damageHits"],s[@"damageIncapacitations"],s[@"damageKills"],[s[@"primaryTargetHealth"] doubleValue],s[@"primaryTargetCombatState"],[s[@"primaryTargetBleedingPerSecond"] doubleValue],
        [s[@"primaryTargetHelmetArmorJoules"] doubleValue],[s[@"primaryTargetTorsoArmorJoules"] doubleValue],s[@"damageArmorHits"],s[@"damageBleedTransitions"]];
    [report appendFormat:@"Combatant Authority total %@ | active %@ wounded %@ incapacitated %@ dead %@ removed %@ | player HP %.1f state %@ bleed %.2f/s | AI shots %@ player impacts %@ friendly-fire denials %@\n",
        s[@"combatantCount"],s[@"combatantActive"],s[@"combatantWounded"],s[@"combatantIncapacitated"],s[@"combatantDead"],s[@"combatantRemoved"],[s[@"playerHealth"] doubleValue],s[@"playerCombatState"],[s[@"playerBleedingPerSecond"] doubleValue],
        s[@"aiShotsFired"],s[@"aiTargetImpacts"],s[@"friendlyFireDenials"]];
    [report appendFormat:@"Ballistics spawned %@ worldContacts %@ terminalWorld %@ targetImpacts %@ penetrations %@ ricochets %@\n",
        s[@"projectilesSpawned"],s[@"worldImpacts"],s[@"terminalWorldImpacts"],s[@"targetImpacts"],s[@"penetrations"],s[@"ricochets"]];
    [report appendFormat:@"AI active %@ peak %@ LOS %@ peakLOS %@ hearing %@ engaged %@ order %@ threat %.2f decisions %@\n",
        s[@"aiActive"],s[@"aiPeakActive"],s[@"aiLOS"],s[@"aiPeakLOS"],s[@"aiHearing"],s[@"aiEngaged"],s[@"aiSquadOrder"],[s[@"aiHighestThreat"] doubleValue],s[@"aiDecisions"]];
    [report appendFormat:@"010-C suppression active %@ | candidate checks latest slice %@/%@ | accepted exposure observations %@ | budget-truncated segments %@ (session)\n",
        s[@"aiSuppressed"],s[@"aiSuppressionChecks"],s[@"aiSuppressionCheckCap"],s[@"aiSuppressionObservations"],s[@"aiSuppressionBudgetDrops"]];
    [report appendFormat:@"010-D injury recovering %@ | local loss concern %@ | injury reactions %@ witnessed loss-recipient pairs %@ (session) | witness checks latest slice %@/%@ | budget-truncated loss events %@ | consumed damage sequence %@\n",
        s[@"aiRecovering"],s[@"aiConcerned"],s[@"aiInjuryReactions"],s[@"aiWitnessedLosses"],s[@"aiLossChecks"],s[@"aiLossCheckCap"],s[@"aiLossBudgetDrops"],s[@"aiObservedDamageSequence"]];
    [report appendFormat:@"Audio cues %@ steps %@ shots O/I %@/%@ crack %@ near %@ | FX active %@ spawned %@ dropped %@ | presentationDrop %@ snapshotDrop %@\n",
        s[@"audioCues"],s[@"audioFootsteps"],s[@"audioOutdoorShots"],s[@"audioIndoorShots"],s[@"audioBulletCracks"],s[@"audioNearMisses"],s[@"fxActive"],s[@"fxSpawned"],s[@"fxDropped"],s[@"audioPresentationDrops"],s[@"audioCueSnapshotDrops"]];
    [report appendFormat:@"Visibility F/R/M/D %@/%@/%@/%@ peak %@/%@/%@/%@ occluded %@ demoted %@\n",
        s[@"visibilityFull"],s[@"visibilityReduced"],s[@"visibilityMinimal"],s[@"visibilityDormant"],s[@"visibilityPeakFull"],s[@"visibilityPeakReduced"],s[@"visibilityPeakMinimal"],s[@"visibilityPeakDormant"],s[@"visibilityOccluded"],s[@"visibilityBudgetDemotions"]];
    [report appendFormat:@"Integrity JRN %@ committed %@ rejected %@ rollback %@ simRollback %@\nGameplayDenials cooldown %@ reloading %@ obstructed %@ empty %@ sprintRecovery %@ capacity %@ reloadInvalid %@ autoReload %@\n",
        [s[@"journalValid"] boolValue]?@"OK":@"FAIL",s[@"commandsCommitted"],s[@"commandsRejected"],s[@"commandsRolledBack"],s[@"simulationInvariantRollbacks"],s[@"denyFireCooldown"],s[@"denyFireReloading"],s[@"denyFireObstructed"],s[@"denyFireEmpty"],s[@"denyFireSprintRecovery"],s[@"denyProjectileCapacity"],s[@"denyReloadInvalid"],s[@"autoReloadStarted"]];
    [report appendFormat:@"Validity journal %@ world %@ observatory %@ queue %@ weapon %@ ballistics %@ damage %@ visibility %@ AI %@ audio %@\n",
        [s[@"journalValid"] boolValue]?@"OK":@"FAIL",[s[@"worldValid"] boolValue]?@"OK":@"FAIL",[s[@"observatoryValid"] boolValue]?@"OK":@"FAIL",[s[@"queueValid"] boolValue]?@"OK":@"FAIL",[s[@"weaponValid"] boolValue]?@"OK":@"FAIL",[s[@"ballisticsValid"] boolValue]?@"OK":@"FAIL",[s[@"damageValid"] boolValue]?@"OK":@"FAIL",[s[@"visibilityValid"] boolValue]?@"OK":@"FAIL",[s[@"tacticalAIValid"] boolValue]?@"OK":@"FAIL",[s[@"audioFXValid"] boolValue]?@"OK":@"FAIL"];
    [report appendFormat:@"Renderer frames %@ misses %@ CPU avg %.3fms max %.3fms\nTiming lockWait avg %.3fms max %.3fms | coreCritical avg %.3fms max %.3fms | callback avg %.2fms max %.2fms >budget %@ >50ms %@ >100ms %@ >250ms %@ lifecycleResets %@\n",
        s[@"renderedFrames"],s[@"drawableMisses"],[s[@"renderCpuAverageMs"] doubleValue],[s[@"renderCpuMaxMs"] doubleValue],[s[@"coreLockWaitAverageMs"] doubleValue],[s[@"coreLockWaitMaxMs"] doubleValue],[s[@"coreCriticalAverageMs"] doubleValue],[s[@"coreCriticalMaxMs"] doubleValue],[s[@"callbackGapAverageMs"] doubleValue],[s[@"callbackGapMaxMs"] doubleValue],s[@"callbackGapsOverBudget"],s[@"callbackGapsOver50ms"],s[@"callbackGapsOver100ms"],s[@"callbackGapsOver250ms"],s[@"lifecycleTimingResets"]];
    [report appendFormat:@"Lifecycle resign %@ background %@ foreground %@ active %@ | memoryWarnings %@\n",
        s[@"lifecycleWillResignActive"],s[@"lifecycleDidEnterBackground"],s[@"lifecycleWillEnterForeground"],s[@"lifecycleDidBecomeActive"],s[@"memoryWarningEvents"]];
    [report appendFormat:@"009-H slice window avg %.3fms max %.3fms >20ms %@ (session %@) | projectile contacts %@ terminal %@ target %@ | AI decisions %@\nBlackBox preSpike session %@ causes callback %@ catchUp %@ slice %@ | retained callback %@ catchUp %@ slice %@\n",
        [s[@"simulationSliceAverageMs"] doubleValue],[s[@"simulationSliceMaxMs"] doubleValue],s[@"windowSimulationSlicesOver20ms"],s[@"simulationSlicesOver20ms"],s[@"projectileContacts"],s[@"projectileTerminalContacts"],s[@"projectileTargetContacts"],s[@"aiDecisions"],s[@"preSpikeBlackBoxFrames"],s[@"preSpikeCallbackFrames"],s[@"preSpikeCatchUpFrames"],s[@"preSpikeSimulationFrames"],s[@"retainedPreSpikeCallbackFrames"],s[@"retainedPreSpikeCatchUpFrames"],s[@"retainedPreSpikeSimulationFrames"]];
    [report appendFormat:@"Source %@\n010-E capture wait %@ms held %@ms finish %@ms | capture %@ bytes / core %@ bytes\nLoad coverage: 31 capable AI + capable player %.2fs; with >=64 projectiles %.2fs (not device certification)\n",
        s[@"sourceCommit"],s[@"diagnosticCaptureWaitMs"],s[@"diagnosticCaptureMs"],s[@"diagnosticFinishMs"],s[@"diagnosticCaptureBytes"],s[@"engineCoreBytes"],[s[@"fullCombatantLoadSeconds"] doubleValue],[s[@"combinedLoadSeconds"] doubleValue]];
    [report appendFormat:@"Diagnostics hardMask 0x%llx coverageMask 0x%llx (hard bits integrity=1 telemetry=2 callback=4 catchUp=8 slice=10 input=20 presentation=40 fx=80 drawable=100 thermalCritical=200 memory=400; coverage bits short=1 noAI=2 noFull32Load=4 noProjectile=8) thermalFallbackFrames %@\nStateHash %@\nJournalHead %@\n",
        [s[@"diagnosticProblemMask"] unsignedLongLongValue],[s[@"acceptanceCoverageMask"] unsignedLongLongValue],s[@"thermalFallbackFrames"],s[@"stateHash"],s[@"journalHead"]];
    return report;
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
    const double callbackBudgetMs = _presentationFPS > 0 ? 1500.0 / static_cast<double>(_presentationFPS) : 25.0;
    if (!suppressGap) {
        _callbackGapMaxMilliseconds = std::max(_callbackGapMaxMilliseconds, rawGapMs);
        ++_callbackSamples;
        _callbackGapTotalMilliseconds += rawGapMs;
        if (rawGapMs > callbackBudgetMs) ++_callbackGapsOverBudget;
        if (rawGapMs > 50.0) ++_callbackGapsOver50ms;
        if (rawGapMs > 100.0) ++_callbackGapsOver100ms;
        if (rawGapMs > 250.0) ++_callbackGapsOver250ms;
        if (_presentationFPS == 30) ++_thermalFallbackFrames;
    }
    os_unfair_lock_unlock(&_telemetryLock);
    const double simulationDelta = suppressGap ? 0.0 : rawDelta;

    metse::EngineSnapshot state{};
    std::array<metse::WorldObstacle, metse::WorldCollisionCore::kMaxObstacles> obstacles{};
    std::array<metse::Projectile, metse::BallisticsCore::kMaxProjectiles> projectiles{};
    std::array<metse::DamageTarget, metse::DamageCore::kMaxTargets> targets{};
    std::array<metse::VisibilityEntity, metse::VisibilityCore::kMaxEntities> visibilityEntities{};
    std::array<metse::TacticalAgentState, metse::TacticalAICore::kMaxAgents> tacticalAgents{};
    std::array<metse::FXInstance, metse::AudioFXCore::kFXCapacity> effects{};
    std::array<metse::AudioCue, metse::AudioFXCore::kCueCapacity> pendingAudioCues{};
    std::size_t obstacleCount=0,targetCount=0,visibilityCount=0,tacticalAgentCount=0,pendingAudioCueCount=0;
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
    tacticalAgents = _core.tacticalAI().agents();
    tacticalAgentCount = _core.tacticalAI().agentCount();
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
    pass.depthAttachment.clearDepth = 1.0;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    METSEFrameUniforms uniforms{};
    float muzzleFlash=0.0f;
    NSUInteger effectWrite=0;
    std::array<METSEBallisticFXRenderState, kNativeBallisticFXCap> nativeBallisticFX{};
    NSUInteger nativeBallisticFXWrite=0;
    for(const auto& projectile:projectiles){
        if(!projectile.active||nativeBallisticFXWrite>=kNativeBallisticFXCap)continue;
        nativeBallisticFX[nativeBallisticFXWrite++]={
            (float)projectile.position.x,(float)projectile.position.y,(float)projectile.position.z,
            (float)projectile.velocity.x,(float)projectile.velocity.y,(float)projectile.velocity.z,
            .012f,1.0f,0,1
        };
    }
    for(const auto& effect:effects){
        if(!effect.active||effect.lifetimeSeconds<=0.0) continue;
        const float remaining=static_cast<float>(
            std::clamp(1.0-effect.ageSeconds/effect.lifetimeSeconds,0.0,1.0));
        const double localDX=effect.position.x-state.playerX;
        const double localDY=effect.position.y-(state.playerY+state.cameraHeight);
        const double localDZ=effect.position.z-state.playerZ;
        const bool localMuzzle=effect.kind==metse::FXKind::MuzzleFlash&&
            localDX*localDX+localDY*localDY+localDZ*localDZ<1.0;
        if(localMuzzle){
            muzzleFlash=std::max(muzzleFlash,remaining*static_cast<float>(effect.intensity));
            continue;
        }
        if(nativeBallisticFXWrite<kNativeBallisticFXCap){
            const bool muzzle=effect.kind==metse::FXKind::MuzzleFlash;
            nativeBallisticFX[nativeBallisticFXWrite++]={
                (float)effect.position.x,(float)effect.position.y,(float)effect.position.z,
                (float)effect.velocity.x,(float)effect.velocity.y,(float)effect.velocity.z,
                muzzle ? .11f : (.14f+.08f*(float)effect.intensity),remaining,
                (uint8_t)(muzzle?2:1),(uint8_t)effect.material
            };
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
    std::array<METSEBattlefieldObstacle, kRenderObstacleCap> battlefieldObstacles{};
    for (NSUInteger index = 0; index < obstacleCount && index < kRenderObstacleCap; ++index) {
        const auto &source = obstacles[index];
        battlefieldObstacles[index] = {
            (float)source.minX, (float)source.minY, (float)source.minZ,
            (float)source.maxX, (float)source.maxY, (float)source.maxZ,
            (uint8_t)source.material
        };
    }
    [self.battlefieldRenderer encodeWithEncoder:encoder
                                  commandBuffer:commandBuffer
                                       viewSize:view.drawableSize
                                      obstacles:battlefieldObstacles.data()
                                          count:std::min<NSUInteger>(obstacleCount, kRenderObstacleCap)
                                           minX:(float)minX maxX:(float)maxX
                                           minZ:(float)minZ maxZ:(float)maxZ
                                        cameraX:(float)state.playerX
                                        cameraY:(float)(state.playerY + state.cameraHeight)
                                        cameraZ:(float)state.playerZ
                                            yaw:(float)state.playerYaw
                                          pitch:(float)state.playerPitch
                                           roll:(float)(state.cameraRoll + state.cameraLean)
                                       adsAlpha:(float)state.adsAlpha
                              simulationSeconds:(float)state.simulationSeconds];
    std::array<METSECombatantRenderState, kRenderTargetCap> renderCombatants{};
    const NSUInteger combatantCount=std::min({targetCount,visibilityCount,tacticalAgentCount,
                                               static_cast<std::size_t>(kRenderTargetCap)});
    for(NSUInteger index=0;index<combatantCount;++index){
        const auto &target=targets[index];
        const auto &visibility=visibilityEntities[index];
        const auto &agent=tacticalAgents[index];
        const bool identityMatches=target.id==visibility.id&&target.id==agent.id;
        renderCombatants[index]={
            (float)target.position.x,(float)target.position.y,(float)target.position.z,
            (float)agent.facingYaw,(float)std::clamp(target.health/100.0,0.0,1.0),
            identityMatches?(uint8_t)visibility.tier:(uint8_t)metse::VisibilityTier::Dormant,
            (uint8_t)target.combatState,(uint8_t)agent.action,
            (uint8_t)(identityMatches&&visibility.lineOfSight),target.id
        };
    }
    [self.combatantRenderer encodeWithEncoder:encoder
                                 commandBuffer:commandBuffer
                                      viewSize:view.drawableSize
                                    combatants:renderCombatants.data()
                                         count:combatantCount
                                       cameraX:(float)state.playerX
                                       cameraY:(float)(state.playerY+state.cameraHeight)
                                       cameraZ:(float)state.playerZ
                                           yaw:(float)state.playerYaw
                                         pitch:(float)state.playerPitch
                                          roll:(float)(state.cameraRoll+state.cameraLean)
                                      adsAlpha:(float)state.adsAlpha
                             simulationSeconds:(float)state.simulationSeconds];
    [self.ballisticFXRenderer encodeWithEncoder:encoder
                                   commandBuffer:commandBuffer
                                        viewSize:view.drawableSize
                                       instances:nativeBallisticFX.data()
                                           count:nativeBallisticFXWrite
                                         cameraX:(float)state.playerX
                                         cameraY:(float)(state.playerY+state.cameraHeight)
                                         cameraZ:(float)state.playerZ
                                             yaw:(float)state.playerYaw
                                           pitch:(float)state.playerPitch
                                            roll:(float)(state.cameraRoll+state.cameraLean)
                                        adsAlpha:(float)state.adsAlpha
                               simulationSeconds:(float)state.simulationSeconds];
    [self.viewmodelRenderer encodeWithEncoder:encoder
                                    viewSize:view.drawableSize
                                    adsAlpha:(float)state.adsAlpha
                                       swayX:(float)state.weaponSwayX
                                       swayY:(float)state.weaponSwayY
                                 recoilPitch:(float)state.recoilPitch
                                   recoilYaw:(float)state.recoilYaw
                                  cameraRoll:(float)(state.cameraRoll + state.cameraLean)
                                  obstructed:state.weaponObstructed
                              reloadRemaining:(float)state.reloadRemaining
                             simulationSeconds:(float)state.simulationSeconds];
    [encoder setRenderPipelineState:self.overlayPipeline];
    [encoder setDepthStencilState:nil];
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
