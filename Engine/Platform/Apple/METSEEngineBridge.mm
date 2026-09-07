#import "METSEEngineBridge.h"
#import <QuartzCore/QuartzCore.h>
#include "../../Core/METSEEngineCore.hpp"

struct METSEFrameUniforms {
    float time;
    vector_float2 resolution;
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
}

- (instancetype)initWithView:(MTKView *)view {
    self = [super init];
    if (!self) return nil;

    _metalView = view;
    _lastFrameTime = CACurrentMediaTime();

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSAssert(device != nil, @"METSE requires a Metal-capable device");
    view.device = device;
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.preferredFramesPerSecond = 60;
    view.enableSetNeedsDisplay = NO;
    view.paused = NO;

    _commandQueue = [device newCommandQueue];

    id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLFunction> vertex = [library newFunctionWithName:@"metseVertex"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"metseFragment"];
    NSAssert(vertex != nil && fragment != nil, @"METSE Metal shaders are missing");

    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = vertex;
    descriptor.fragmentFunction = fragment;
    descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;

    NSError *error = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    NSAssert(_pipeline != nil, @"METSE pipeline creation failed: %@", error);

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
}

- (NSString *)statusString {
    const auto &s = _core.snapshot();
    return [NSString stringWithFormat:@"Native Metal • tick %llu • %.1fs",
            (unsigned long long)s.simulationTick, s.simulationSeconds];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {}

- (void)drawInMTKView:(MTKView *)view {
    if (!_running) return;

    const CFTimeInterval now = CACurrentMediaTime();
    _core.advance(now - _lastFrameTime);
    _lastFrameTime = now;

    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline || !self.commandQueue) return;

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:self.pipeline];

    METSEFrameUniforms uniforms;
    uniforms.time = (float)_core.snapshot().simulationSeconds;
    uniforms.resolution = (vector_float2){(float)view.drawableSize.width, (float)view.drawableSize.height};
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];

    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

@end
