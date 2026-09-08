#import "METSEEngineBridge.h"
#import <QuartzCore/QuartzCore.h>
#import <os/lock.h>
#import <simd/simd.h>
#include "../../Core/METSEEngineCore.hpp"
#include <algorithm>

static constexpr NSUInteger kRenderObstacleCap = metse::WorldCollisionCore::kMaxObstacles;
static constexpr NSUInteger kRenderProjectileCap = 8;
static constexpr NSUInteger kRenderTargetCap = 4;

struct METSEFrameUniforms {
    vector_float4 timing;
    vector_float4 camera;
    vector_float4 character;
    vector_float4 weapon;
    vector_float4 weapon2;
    vector_float4 worldMeta;
    vector_float4 worldExtra;
    vector_float4 obstacleBounds[kRenderObstacleCap];
    vector_float4 obstacleMeta[kRenderObstacleCap];
    vector_float4 projectilePositions[kRenderProjectileCap];
    vector_float4 targetData[kRenderTargetCap];
};
static_assert(sizeof(METSEFrameUniforms) % 16 == 0, "Metal uniform ABI must remain 16-byte aligned");

@interface METSEEngineBridge ()
@property(nonatomic, weak) MTKView *metalView;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@end

@implementation METSEEngineBridge {
    metse::EngineCore _core;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _nextThermalCheck;
    BOOL _running;
    uint64_t _lastRenderedShot;
    float _muzzleFlash;
    os_unfair_lock _coreLock;
    os_unfair_lock _telemetryLock;
    uint64_t _renderedFrames;
    uint64_t _drawableMisses;
    double _renderCpuTotalMilliseconds;
    double _renderCpuMaxMilliseconds;
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
    view.delegate = self;
    return self;
}

- (void)start { _running = YES; _lastFrameTime = CACurrentMediaTime(); self.metalView.paused = NO; }
- (void)stop {
    _running = NO;
    self.metalView.paused = YES;
    os_unfair_lock_lock(&_coreLock);
    _core.setMovementInput(0, 0);
    _core.setSprintHeld(false);
    _core.setAimHeld(false);
    os_unfair_lock_unlock(&_coreLock);
}
- (void)setMoveForward:(float)forward strafe:(float)strafe { os_unfair_lock_lock(&_coreLock); _core.setMovementInput(forward, strafe); os_unfair_lock_unlock(&_coreLock); }
- (void)addLookYaw:(float)yaw pitch:(float)pitch { os_unfair_lock_lock(&_coreLock); _core.addLookInput(yaw, pitch); os_unfair_lock_unlock(&_coreLock); }
- (void)setSprintHeld:(BOOL)held { os_unfair_lock_lock(&_coreLock); _core.setSprintHeld(held); os_unfair_lock_unlock(&_coreLock); }
- (void)setAimHeld:(BOOL)held { os_unfair_lock_lock(&_coreLock); _core.setAimHeld(held); os_unfair_lock_unlock(&_coreLock); }
- (void)cycleStance { os_unfair_lock_lock(&_coreLock); _core.cycleStance(); os_unfair_lock_unlock(&_coreLock); }
- (void)triggerFire { os_unfair_lock_lock(&_coreLock); BOOL queued = _core.triggerFire(); os_unfair_lock_unlock(&_coreLock); if (queued) _muzzleFlash = 1.0f; }
- (void)reloadWeapon { os_unfair_lock_lock(&_coreLock); _core.reloadWeapon(); os_unfair_lock_unlock(&_coreLock); }

static NSString *METSEStanceName(metse::CharacterStance stance) {
    switch (stance) { case metse::CharacterStance::Standing: return @"STAND"; case metse::CharacterStance::Crouched: return @"CROUCH"; case metse::CharacterStance::Prone: return @"PRONE"; }
    return @"UNKNOWN";
}
static NSString *METSEGaitName(metse::CharacterGait gait) {
    switch (gait) { case metse::CharacterGait::Idle:return @"IDLE";case metse::CharacterGait::Walk:return @"WALK";case metse::CharacterGait::Tactical:return @"TACTICAL";case metse::CharacterGait::Jog:return @"JOG";case metse::CharacterGait::Sprint:return @"SPRINT";case metse::CharacterGait::Crouch:return @"CROUCH";case metse::CharacterGait::Crawl:return @"CRAWL"; }
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
    os_unfair_lock_lock(&_coreLock);
    const auto state = _core.snapshot();
    const auto diagnostics = _core.diagnostics();
    os_unfair_lock_unlock(&_coreLock);
    return [NSString stringWithFormat:@"%@/%@ %.1fm/s • %u/%u • ADS %.0f%% • P %u • Q %llu • JRN %@ • BB %llu",
            METSEStanceName(state.stance), METSEGaitName(state.gait), state.horizontalSpeed,
            state.ammoInMagazine, state.reserveAmmo, state.adsAlpha * 100.0,
            state.activeProjectiles, (unsigned long long)diagnostics.inputQueueDepth,
            diagnostics.journalValid ? @"OK" : @"FAIL", (unsigned long long)diagnostics.retainedBlackBoxFrames];
}

- (NSDictionary<NSString *, id> *)observatorySnapshot {
    os_unfair_lock_lock(&_coreLock);
    const auto s = _core.snapshot();
    const auto d = _core.diagnostics();
    os_unfair_lock_unlock(&_coreLock);
    const auto o = d.observatory;
    const auto stateHash = metse::sha256Hex(d.stateHash);
    const auto journalHead = metse::sha256Hex(d.journalHead);
    os_unfair_lock_lock(&_telemetryLock);
    uint64_t rendered = _renderedFrames, misses = _drawableMisses;
    double cpuTotal = _renderCpuTotalMilliseconds, cpuMax = _renderCpuMaxMilliseconds;
    os_unfair_lock_unlock(&_telemetryLock);
    double cpuAvg = rendered ? cpuTotal / (double)rendered : 0.0;
    return @{
        @"version":@"0.3.0", @"build":@8, @"presentationFPS":@(_presentationFPS),
        @"simulationTick":@(s.simulationTick), @"simulationSeconds":@(s.simulationSeconds),
        @"stance":METSEStanceName(s.stance), @"gait":METSEGaitName(s.gait), @"speed":@(s.horizontalSpeed),
        @"playerX":@(s.playerX), @"playerY":@(s.playerY), @"playerZ":@(s.playerZ), @"grounded":@(s.grounded),
        @"ammo":@(s.ammoInMagazine), @"reserveAmmo":@(s.reserveAmmo), @"adsAlpha":@(s.adsAlpha), @"reloading":@(s.reloading), @"reloadRemaining":@(s.reloadRemaining), @"weaponObstructed":@(s.weaponObstructed),
        @"shotsFired":@(s.shotsFired), @"activeProjectiles":@(s.activeProjectiles), @"damageHits":@(s.damageHits), @"damageKills":@(s.damageKills), @"primaryTargetHealth":@(s.primaryTargetHealth),
        @"averageFrameMs":@(o.averageFrameMilliseconds), @"p95FrameMs":@(o.p95FrameMilliseconds), @"p99FrameMs":@(o.p99FrameMilliseconds), @"maxFrameMs":@(o.maxFrameMilliseconds), @"estimatedFPS":@(o.estimatedFPS), @"onePercentLowFPS":@(o.onePercentLowFPS), @"pointOnePercentLowFPS":@(o.pointOnePercentLowFPS),
        @"framesOver20ms":@(o.framesOver20ms), @"framesOver33ms":@(o.framesOver33ms), @"catchUpClampedFrames":@(o.catchUpClampedFrames),
        @"queueDepth":@(d.inputQueueDepth), @"queueHighWatermark":@(d.inputQueue.highWatermark), @"queueCoalesced":@(d.inputQueue.coalesced), @"queueEvicted":@(d.inputQueue.evictedCoalescible), @"queueRejectedCritical":@(d.inputQueue.rejectedCritical), @"queueRejectedInvalid":@(d.inputQueue.rejectedInvalid),
        @"projectilesSpawned":@(d.ballistics.spawned), @"worldImpacts":@(d.ballistics.worldImpacts), @"targetImpacts":@(d.ballistics.targetImpacts), @"penetrations":@(d.ballistics.penetrations),
        @"visibilityFull":@(d.visibility.full), @"visibilityReduced":@(d.visibility.reduced), @"visibilityMinimal":@(d.visibility.minimal), @"visibilityDormant":@(d.visibility.dormant),
        @"collisionContacts":@(d.sessionCollisionContacts), @"worldObstacleCount":@(d.worldObstacleCount),
        @"journalValid":@(d.journalValid), @"worldValid":@(d.worldValid), @"observatoryValid":@(d.observatoryValid), @"queueValid":@(d.inputQueueValid), @"weaponValid":@(d.weaponValid), @"ballisticsValid":@(d.ballisticsValid), @"damageValid":@(d.damageValid), @"visibilityValid":@(d.visibilityValid),
        @"commandsCommitted":@(d.integrity.commandsCommitted), @"commandsRejected":@(d.integrity.commandsRejected), @"commandsRolledBack":@(d.integrity.commandsRolledBack), @"simulationInvariantRollbacks":@(d.simulationInvariantRollbacks), @"blackBoxFrames":@(d.retainedBlackBoxFrames),
        @"stateHash":[NSString stringWithUTF8String:stateHash.c_str()], @"journalHead":[NSString stringWithUTF8String:journalHead.c_str()],
        @"renderedFrames":@(rendered), @"drawableMisses":@(misses), @"renderCpuAverageMs":@(cpuAvg), @"renderCpuMaxMs":@(cpuMax)
    };
}

- (NSString *)observatoryReportText {
    NSDictionary *s = [self observatorySnapshot];
    return [NSString stringWithFormat:
        @"METSE OBSERVATORY V2\nVersion %@ Build %@\nPresentation %@ FPS / Simulation 60 Hz\nTick %@ / %.2fs\nFrame %.1f FPS avg %.2fms p95 %.2f p99 %.2f max %.2f | 1%% low %.1f | 0.1%% low %.1f\nInput Q %@ peak %@ coalesced %@ evicted %@ rejectedCritical %@ rejectedInvalid %@\nWeapon %@/%@ ADS %.0f%% reload %@ obstructed %@\nCombat shots %@ projectiles %@ hits %@ kills %@ targetHP %.1f\nBallistics spawned %@ worldImpacts %@ targetImpacts %@ penetrations %@\nVisibility F/R/M/D %@/%@/%@/%@\nIntegrity JRN %@ committed %@ rejected %@ rollback %@ simRollback %@\nRenderer frames %@ misses %@ CPU avg %.3fms max %.3fms\nStateHash %@\nJournalHead %@\n",
        s[@"version"],s[@"build"],s[@"presentationFPS"],s[@"simulationTick"],[s[@"simulationSeconds"] doubleValue],
        [s[@"estimatedFPS"] doubleValue],[s[@"averageFrameMs"] doubleValue],[s[@"p95FrameMs"] doubleValue],[s[@"p99FrameMs"] doubleValue],[s[@"maxFrameMs"] doubleValue],[s[@"onePercentLowFPS"] doubleValue],[s[@"pointOnePercentLowFPS"] doubleValue],
        s[@"queueDepth"],s[@"queueHighWatermark"],s[@"queueCoalesced"],s[@"queueEvicted"],s[@"queueRejectedCritical"],s[@"queueRejectedInvalid"],
        s[@"ammo"],s[@"reserveAmmo"],[s[@"adsAlpha"] doubleValue]*100.0,[s[@"reloading"] boolValue]?@"YES":@"NO",[s[@"weaponObstructed"] boolValue]?@"YES":@"NO",
        s[@"shotsFired"],s[@"activeProjectiles"],s[@"damageHits"],s[@"damageKills"],[s[@"primaryTargetHealth"] doubleValue],
        s[@"projectilesSpawned"],s[@"worldImpacts"],s[@"targetImpacts"],s[@"penetrations"],
        s[@"visibilityFull"],s[@"visibilityReduced"],s[@"visibilityMinimal"],s[@"visibilityDormant"],
        [s[@"journalValid"] boolValue]?@"OK":@"FAIL",s[@"commandsCommitted"],s[@"commandsRejected"],s[@"commandsRolledBack"],s[@"simulationInvariantRollbacks"],
        s[@"renderedFrames"],s[@"drawableMisses"],[s[@"renderCpuAverageMs"] doubleValue],[s[@"renderCpuMaxMs"] doubleValue],s[@"stateHash"],s[@"journalHead"]];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size { (void)view; (void)size; }

- (void)drawInMTKView:(MTKView *)view {
    if (!_running) return;
    CFTimeInterval frameStart = CACurrentMediaTime();
    [self updateThermalPresentationIfNeeded:frameStart];
    metse::EngineSnapshot state{};
    std::array<metse::WorldObstacle, metse::WorldCollisionCore::kMaxObstacles> obstacles{};
    std::array<metse::Projectile, metse::BallisticsCore::kMaxProjectiles> projectiles{};
    std::array<metse::DamageTarget, metse::DamageCore::kMaxTargets> targets{};
    std::size_t obstacleCount=0,targetCount=0;
    double minX=0,maxX=0,minZ=0,maxZ=0;
    os_unfair_lock_lock(&_coreLock);
    _core.advance(frameStart - _lastFrameTime);
    state = _core.snapshot();
    obstacles = _core.worldObstacles();
    obstacleCount = _core.worldObstacleCount();
    const auto &world = _core.worldCollision(); minX=world.minWorldX();maxX=world.maxWorldX();minZ=world.minWorldZ();maxZ=world.maxWorldZ();
    projectiles = _core.projectiles();
    targets = _core.damageTargets();
    targetCount = _core.damageTargetCount();
    os_unfair_lock_unlock(&_coreLock);
    _lastFrameTime = frameStart;

    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline || !self.commandQueue) { os_unfair_lock_lock(&_telemetryLock); ++_drawableMisses; os_unfair_lock_unlock(&_telemetryLock); return; }
    if (state.shotsFired != _lastRenderedShot) { _lastRenderedShot = state.shotsFired; _muzzleFlash = 1.0f; }
    _muzzleFlash *= 0.80f;

    METSEFrameUniforms u{};
    u.timing=(vector_float4){(float)state.simulationSeconds,(float)view.drawableSize.width,(float)view.drawableSize.height,_muzzleFlash};
    u.camera=(vector_float4){(float)state.playerX,(float)state.playerZ,(float)state.playerYaw,(float)state.playerPitch};
    u.character=(vector_float4){(float)state.cameraHeight,(float)state.playerY,(float)state.horizontalSpeed,(float)state.cameraRoll};
    u.weapon=(vector_float4){(float)state.adsAlpha,(float)state.recoilPitch,(float)state.recoilYaw,state.weaponObstructed?1.0f:0.0f};
    u.weapon2=(vector_float4){(float)state.weaponSwayX,(float)state.weaponSwayY,(float)state.ammoInMagazine,(float)state.reloadRemaining};
    u.worldMeta=(vector_float4){(float)obstacleCount,(float)minX,(float)maxX,(float)minZ};
    u.worldExtra=(vector_float4){(float)maxZ,(float)std::min<std::size_t>(targetCount,kRenderTargetCap),0.0f,(float)state.cameraLean};
    for (NSUInteger i=0;i<kRenderObstacleCap;++i) { const auto&o=obstacles[i];u.obstacleBounds[i]=(vector_float4){(float)o.minX,(float)o.minZ,(float)o.maxX,(float)o.maxZ};u.obstacleMeta[i]=(vector_float4){(float)o.minY,(float)o.maxY,(float)static_cast<std::uint8_t>(o.material),0}; }
    NSUInteger projectileWrite=0; for (const auto&p:projectiles) { if(!p.active||projectileWrite>=kRenderProjectileCap)continue;u.projectilePositions[projectileWrite++]=(vector_float4){(float)p.position.x,(float)p.position.y,(float)p.position.z,1}; }
    u.worldExtra.z=(float)projectileWrite;
    for (NSUInteger i=0;i<MIN(targetCount,kRenderTargetCap);++i) { const auto&t=targets[i];u.targetData[i]=(vector_float4){(float)t.position.x,(float)t.position.z,(float)t.health,t.alive?1.0f:0.0f}; }

    id<MTLCommandBuffer> cb=[self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:pass];
    [e setRenderPipelineState:self.pipeline]; [e setFragmentBytes:&u length:sizeof(u) atIndex:0]; [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding]; [cb presentDrawable:drawable]; [cb commit];
    double cpuMs=(CACurrentMediaTime()-frameStart)*1000.0; os_unfair_lock_lock(&_telemetryLock);++_renderedFrames;_renderCpuTotalMilliseconds+=cpuMs;_renderCpuMaxMilliseconds=std::max(_renderCpuMaxMilliseconds,cpuMs);os_unfair_lock_unlock(&_telemetryLock);
}
@end
