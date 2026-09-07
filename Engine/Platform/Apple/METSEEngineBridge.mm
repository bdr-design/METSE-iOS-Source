#import "METSEEngineBridge.h"
#import <QuartzCore/QuartzCore.h>
#import <os/lock.h>
#import <simd/simd.h>
#include "../../Core/METSEEngineCore.hpp"
#include <algorithm>

struct METSEFrameUniforms {
    vector_float4 timing;
    vector_float4 camera;
    vector_float4 state;
    vector_float4 character;
    vector_float4 worldMeta;
    vector_float4 worldExtra;
    vector_float4 obstacles[metse::WorldCollisionCore::kMaxObstacles];
    vector_float4 obstacleHeightsA;
    vector_float4 obstacleHeightsB;
};

@interface METSEEngineBridge ()
@property(nonatomic, weak) MTKView *metalView;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@end

@implementation METSEEngineBridge {
    metse::EngineCore _core;
    CFTimeInterval _lastFrameTime;
    BOOL _running;
    uint64_t _lastRenderedShot;
    float _muzzleFlash;
    os_unfair_lock _telemetryLock;
    uint64_t _renderedFrames;
    uint64_t _drawableMisses;
    double _renderCpuTotalMilliseconds;
    double _renderCpuMaxMilliseconds;
}

- (instancetype)initWithView:(MTKView *)view {
    self = [super init];
    if (!self) return nil;

    _metalView = view;
    _lastFrameTime = CACurrentMediaTime();
    _telemetryLock = OS_UNFAIR_LOCK_INIT;

    id<MTLDevice> device = view.device ?: MTLCreateSystemDefaultDevice();
    NSAssert(device != nil, @"METSE requires Metal");
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
    NSAssert(vertex && fragment, @"METSE Metal shaders missing");
    if (!vertex || !fragment || !_commandQueue) return nil;

    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = vertex;
    descriptor.fragmentFunction = fragment;
    descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    NSError *error = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    NSAssert(_pipeline, @"METSE pipeline failed: %@", error);
    if (!_pipeline) return nil;

    view.delegate = self;
    return self;
}

- (void)start {
    _running = YES;
    _lastFrameTime = CACurrentMediaTime();
    self.metalView.paused = NO;
}

- (void)stop {
    _running = NO;
    self.metalView.paused = YES;
    _core.setMovementInput(0, 0);
    _core.setSprintHeld(false);
}

- (void)setMoveForward:(float)forward strafe:(float)strafe { _core.setMovementInput(forward, strafe); }
- (void)addLookYaw:(float)yaw pitch:(float)pitch { _core.addLookInput(yaw, pitch); }
- (void)setSprintHeld:(BOOL)held { _core.setSprintHeld(held); }
- (void)cycleStance { _core.cycleStance(); }
- (void)triggerFire { _core.triggerFire(); _muzzleFlash = 1.0f; }

static NSString *METSEStanceName(metse::CharacterStance stance) {
    switch (stance) {
        case metse::CharacterStance::Standing: return @"STAND";
        case metse::CharacterStance::Crouched: return @"CROUCH";
        case metse::CharacterStance::Prone: return @"PRONE";
    }
    return @"UNKNOWN";
}

static NSString *METSEGaitName(metse::CharacterGait gait) {
    switch (gait) {
        case metse::CharacterGait::Idle: return @"IDLE";
        case metse::CharacterGait::Walk: return @"WALK";
        case metse::CharacterGait::Tactical: return @"TACTICAL";
        case metse::CharacterGait::Jog: return @"JOG";
        case metse::CharacterGait::Sprint: return @"SPRINT";
        case metse::CharacterGait::Crouch: return @"CROUCH";
        case metse::CharacterGait::Crawl: return @"CRAWL";
    }
    return @"UNKNOWN";
}

- (NSString *)statusString {
    const auto &state = _core.snapshot();
    const auto diagnostics = _core.diagnostics();
    NSString *journal = diagnostics.journalValid ? @"JRN OK" : @"JRN FAIL";
    return [NSString stringWithFormat:@"%@ • %@ %.1fm/s • %.1f, %.1f • %@ • COL %llu • BB %llu",
            METSEStanceName(state.stance),
            METSEGaitName(state.gait),
            state.horizontalSpeed,
            state.playerX,
            state.playerZ,
            journal,
            (unsigned long long)diagnostics.sessionCollisionContacts,
            (unsigned long long)diagnostics.retainedBlackBoxFrames];
}

- (NSDictionary<NSString *, id> *)observatorySnapshot {
    const auto &state = _core.snapshot();
    const auto diagnostics = _core.diagnostics();
    const auto &observatory = diagnostics.observatory;
    const auto head = metse::sha256Hex(diagnostics.journalHead);

    os_unfair_lock_lock(&_telemetryLock);
    const uint64_t renderedFrames = _renderedFrames;
    const uint64_t drawableMisses = _drawableMisses;
    const double renderCpuTotal = _renderCpuTotalMilliseconds;
    const double renderCpuMax = _renderCpuMaxMilliseconds;
    os_unfair_lock_unlock(&_telemetryLock);
    const double renderCpuAverage = renderedFrames > 0 ? renderCpuTotal / (double)renderedFrames : 0.0;

    return @{
        @"version": @"0.2.0",
        @"build": @7,
        @"simulationSeconds": @(state.simulationSeconds),
        @"simulationTick": @(state.simulationTick),
        @"stance": METSEStanceName(state.stance),
        @"gait": METSEGaitName(state.gait),
        @"speed": @(state.horizontalSpeed),
        @"playerX": @(state.playerX),
        @"playerY": @(state.playerY),
        @"playerZ": @(state.playerZ),
        @"bodyYaw": @(state.playerBodyYaw),
        @"cameraYaw": @(state.playerYaw),
        @"cameraPitch": @(state.playerPitch),
        @"cameraRoll": @(state.cameraRoll),
        @"cameraHeight": @(state.cameraHeight),
        @"grounded": @(state.grounded),
        @"sprinting": @(state.sprinting),
        @"shotsFired": @(state.shotsFired),
        @"averageFrameMs": @(observatory.averageFrameMilliseconds),
        @"p95FrameMs": @(observatory.p95FrameMilliseconds),
        @"maxFrameMs": @(observatory.maxFrameMilliseconds),
        @"estimatedFPS": @(observatory.estimatedFPS),
        @"framesOver20ms": @(observatory.framesOver20ms),
        @"framesOver33ms": @(observatory.framesOver33ms),
        @"catchUpClampedFrames": @(observatory.catchUpClampedFrames),
        @"observedFrames": @(observatory.observedFrames),
        @"retainedTelemetryFrames": @(observatory.retainedFrames),
        @"distanceTravelled": @(observatory.distanceTravelled),
        @"peakSpeed": @(observatory.peakHorizontalSpeed),
        @"sprintSeconds": @(observatory.sprintSeconds),
        @"airborneSeconds": @(observatory.airborneSeconds),
        @"standingSeconds": @(observatory.standingSeconds),
        @"crouchedSeconds": @(observatory.crouchedSeconds),
        @"proneSeconds": @(observatory.proneSeconds),
        @"stanceTransitions": @(observatory.stanceTransitions),
        @"gaitTransitions": @(observatory.gaitTransitions),
        @"collisionContacts": @(diagnostics.sessionCollisionContacts),
        @"worldObstacleCount": @(diagnostics.worldObstacleCount),
        @"worldValid": @(diagnostics.worldValid),
        @"observatoryValid": @(diagnostics.observatoryValid),
        @"journalValid": @(diagnostics.journalValid),
        @"journalHead": [NSString stringWithUTF8String:head.c_str()],
        @"commandsCommitted": @(diagnostics.integrity.commandsCommitted),
        @"commandsRejected": @(diagnostics.integrity.commandsRejected),
        @"commandsRolledBack": @(diagnostics.integrity.commandsRolledBack),
        @"simulationInvariantRollbacks": @(diagnostics.simulationInvariantRollbacks),
        @"retainedEvents": @(diagnostics.retainedEvents),
        @"retainedCommands": @(diagnostics.retainedCommands),
        @"blackBoxFrames": @(diagnostics.retainedBlackBoxFrames),
        @"renderedFrames": @(renderedFrames),
        @"drawableMisses": @(drawableMisses),
        @"renderCpuAverageMs": @(renderCpuAverage),
        @"renderCpuMaxMs": @(renderCpuMax)
    };
}

- (NSString *)observatoryReportText {
    NSDictionary<NSString *, id> *s = [self observatorySnapshot];
    return [NSString stringWithFormat:
        @"METSE OBSERVATORY REPORT\nVersion %@ Build %@\n"
        @"Runtime: tick %@ / %.2fs\n"
        @"Frame: %.1f FPS | avg %.2fms | p95 %.2fms | max %.2fms | >20ms %@ | >33ms %@ | clamped %@\n"
        @"Character: %@ / %@ | %.2fm/s | pos %.2f, %.2f, %.2f | camera h %.2f roll %.4f | grounded %@\n"
        @"Session: distance %.2fm | peak %.2fm/s | sprint %.2fs | airborne %.2fs | shots %@\n"
        @"World: obstacles %@ | collision contacts %@ | valid %@\n"
        @"Integrity: journal %@ | committed %@ | rejected %@ | rolledBack %@ | simRollbacks %@ | events %@ | commands %@ | blackBox %@\n"
        @"Renderer: frames %@ | drawableMisses %@ | CPU avg %.3fms | CPU max %.3fms\n"
        @"JournalHead: %@\n",
        s[@"version"], s[@"build"], s[@"simulationTick"], [s[@"simulationSeconds"] doubleValue],
        [s[@"estimatedFPS"] doubleValue], [s[@"averageFrameMs"] doubleValue], [s[@"p95FrameMs"] doubleValue], [s[@"maxFrameMs"] doubleValue], s[@"framesOver20ms"], s[@"framesOver33ms"], s[@"catchUpClampedFrames"],
        s[@"stance"], s[@"gait"], [s[@"speed"] doubleValue], [s[@"playerX"] doubleValue], [s[@"playerY"] doubleValue], [s[@"playerZ"] doubleValue], [s[@"cameraHeight"] doubleValue], [s[@"cameraRoll"] doubleValue], [s[@"grounded"] boolValue] ? @"YES" : @"NO",
        [s[@"distanceTravelled"] doubleValue], [s[@"peakSpeed"] doubleValue], [s[@"sprintSeconds"] doubleValue], [s[@"airborneSeconds"] doubleValue], s[@"shotsFired"],
        s[@"worldObstacleCount"], s[@"collisionContacts"], [s[@"worldValid"] boolValue] ? @"YES" : @"NO",
        [s[@"journalValid"] boolValue] ? @"OK" : @"FAIL", s[@"commandsCommitted"], s[@"commandsRejected"], s[@"commandsRolledBack"], s[@"simulationInvariantRollbacks"], s[@"retainedEvents"], s[@"retainedCommands"], s[@"blackBoxFrames"],
        s[@"renderedFrames"], s[@"drawableMisses"], [s[@"renderCpuAverageMs"] doubleValue], [s[@"renderCpuMaxMs"] doubleValue],
        s[@"journalHead"]];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    (void)size;
}

- (void)drawInMTKView:(MTKView *)view {
    if (!_running) return;

    const CFTimeInterval frameStart = CACurrentMediaTime();
    const CFTimeInterval now = frameStart;
    _core.advance(now - _lastFrameTime);
    _lastFrameTime = now;

    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline || !self.commandQueue) {
        os_unfair_lock_lock(&_telemetryLock);
        ++_drawableMisses;
        os_unfair_lock_unlock(&_telemetryLock);
        return;
    }

    const auto &state = _core.snapshot();
    if (state.shotsFired != _lastRenderedShot) {
        _lastRenderedShot = state.shotsFired;
        _muzzleFlash = 1.0f;
    }
    _muzzleFlash *= 0.82f;

    METSEFrameUniforms uniforms{};
    uniforms.timing = (vector_float4){(float)state.simulationSeconds, (float)view.drawableSize.width, (float)view.drawableSize.height, _muzzleFlash};
    uniforms.camera = (vector_float4){(float)state.playerX, (float)state.playerZ, (float)state.playerYaw, (float)state.playerPitch};
    uniforms.state = (vector_float4){(float)state.activeCombatants, (float)state.shotsFired, state.sprinting ? 1.0f : 0.0f, (float)static_cast<std::uint8_t>(state.stance)};
    uniforms.character = (vector_float4){(float)state.cameraHeight, (float)state.playerY, (float)state.horizontalSpeed, state.grounded ? 1.0f : 0.0f};
    const auto &world = _core.worldCollision();
    uniforms.worldMeta = (vector_float4){(float)_core.worldObstacleCount(), (float)world.minWorldX(), (float)world.maxWorldX(), (float)world.minWorldZ()};
    uniforms.worldExtra = (vector_float4){(float)world.maxWorldZ(), (float)state.cameraRoll, 0.0f, 0.0f};

    const auto &obstacles = _core.worldObstacles();
    float heights[metse::WorldCollisionCore::kMaxObstacles] = {};
    for (std::size_t i = 0; i < metse::WorldCollisionCore::kMaxObstacles; ++i) {
        uniforms.obstacles[i] = (vector_float4){(float)obstacles[i].minX, (float)obstacles[i].minZ, (float)obstacles[i].maxX, (float)obstacles[i].maxZ};
        heights[i] = (float)obstacles[i].height;
    }
    uniforms.obstacleHeightsA = (vector_float4){heights[0], heights[1], heights[2], heights[3]};
    uniforms.obstacleHeightsB = (vector_float4){heights[4], heights[5], 0.0f, 0.0f};

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:self.pipeline];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    const double cpuMs = (CACurrentMediaTime() - frameStart) * 1000.0;
    os_unfair_lock_lock(&_telemetryLock);
    ++_renderedFrames;
    _renderCpuTotalMilliseconds += cpuMs;
    _renderCpuMaxMilliseconds = std::max(_renderCpuMaxMilliseconds, cpuMs);
    os_unfair_lock_unlock(&_telemetryLock);
}
@end
