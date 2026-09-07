#import "METSEEngineBridge.h"
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>
#include "../../Core/METSEEngineCore.hpp"

struct METSEFrameUniforms { vector_float4 timing; vector_float4 camera; vector_float4 state; vector_float4 character; };

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
}

- (instancetype)initWithView:(MTKView *)view {
    self = [super init]; if (!self) return nil;
    _metalView = view; _lastFrameTime = CACurrentMediaTime();
    id<MTLDevice> device = view.device ?: MTLCreateSystemDefaultDevice();
    NSAssert(device != nil, @"METSE requires Metal"); if (!device) return nil;
    view.device = device; view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB; view.preferredFramesPerSecond = 60;
    view.enableSetNeedsDisplay = NO; view.paused = NO; view.framebufferOnly = YES;
    _commandQueue = [device newCommandQueue];
    id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLFunction> vertex = [library newFunctionWithName:@"metseVertex"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"metseFragment"];
    NSAssert(vertex && fragment, @"METSE Metal shaders missing"); if (!vertex || !fragment || !_commandQueue) return nil;
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new]; d.vertexFunction = vertex; d.fragmentFunction = fragment; d.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    NSError *error=nil; _pipeline=[device newRenderPipelineStateWithDescriptor:d error:&error]; NSAssert(_pipeline, @"METSE pipeline failed: %@", error); if (!_pipeline) return nil;
    view.delegate=self; return self;
}

- (void)start { _running=YES; _lastFrameTime=CACurrentMediaTime(); self.metalView.paused=NO; }
- (void)stop { _running=NO; self.metalView.paused=YES; _core.setMovementInput(0,0); _core.setSprintHeld(false); }
- (void)setMoveForward:(float)forward strafe:(float)strafe { _core.setMovementInput(forward, strafe); }
- (void)addLookYaw:(float)yaw pitch:(float)pitch { _core.addLookInput(yaw, pitch); }
- (void)setSprintHeld:(BOOL)held { _core.setSprintHeld(held); }
- (void)cycleStance { _core.cycleStance(); }
- (void)triggerFire { _core.triggerFire(); _muzzleFlash = 1.0f; }
- (NSString *)statusString {
    const auto&s=_core.snapshot(); const auto d=_core.diagnostics(); const auto head=metse::sha256Hex(d.journalHead);
    NSString *journal=d.journalValid?@"JRN OK":@"JRN FAIL"; NSString *stance=@"STAND";
    switch(s.stance){case metse::CharacterStance::Standing:stance=@"STAND";break;case metse::CharacterStance::Crouched:stance=@"CROUCH";break;case metse::CharacterStance::Prone:stance=@"PRONE";break;}
    NSString *gait=s.sprinting?@"SPRINT":@"MOVE";
    return [NSString stringWithFormat:@"60Hz • %@/%@ %.1fm/s • %.1f, %.1f • %@ • CP %llu/%llu/%llu • BB %llu • H %.8s",stance,gait,s.horizontalSpeed,s.playerX,s.playerZ,journal,(unsigned long long)d.integrity.commandsCommitted,(unsigned long long)d.integrity.commandsRejected,(unsigned long long)d.integrity.commandsRolledBack,(unsigned long long)d.retainedBlackBoxFrames,head.c_str()];
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size { (void)view; (void)size; }

- (void)drawInMTKView:(MTKView *)view {
    if (!_running) return;
    const CFTimeInterval now=CACurrentMediaTime(); _core.advance(now-_lastFrameTime); _lastFrameTime=now;
    MTLRenderPassDescriptor *pass=view.currentRenderPassDescriptor; id<CAMetalDrawable> drawable=view.currentDrawable; if(!pass||!drawable||!self.pipeline||!self.commandQueue)return;
    const auto&s=_core.snapshot(); if(s.shotsFired!=_lastRenderedShot){_lastRenderedShot=s.shotsFired;_muzzleFlash=1.0f;} _muzzleFlash*=0.82f;
    METSEFrameUniforms u{}; u.timing=(vector_float4){(float)s.simulationSeconds,(float)view.drawableSize.width,(float)view.drawableSize.height,_muzzleFlash};
    u.camera=(vector_float4){(float)s.playerX,(float)s.playerZ,(float)s.playerYaw,(float)s.playerPitch};
    u.state=(vector_float4){(float)s.activeCombatants,(float)s.shotsFired,s.sprinting?1.0f:0.0f,(float)static_cast<std::uint8_t>(s.stance)};
    u.character=(vector_float4){(float)s.cameraHeight,(float)s.playerY,(float)s.horizontalSpeed,s.grounded?1.0f:0.0f};
    id<MTLCommandBuffer> cb=[self.commandQueue commandBuffer]; id<MTLRenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:pass]; [e setRenderPipelineState:self.pipeline]; [e setFragmentBytes:&u length:sizeof(u) atIndex:0]; [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding]; [cb presentDrawable:drawable]; [cb commit];
}
@end
