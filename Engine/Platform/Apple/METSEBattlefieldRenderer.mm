#import "METSEBattlefieldRenderer.h"
#import <dispatch/dispatch.h>
#import <simd/simd.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr NSUInteger kMaximumObstacleInstances = 20;
constexpr NSUInteger kBoundaryInstanceCount = 4;
constexpr NSUInteger kMaximumInstances = 1 + kMaximumObstacleInstances + kBoundaryInstanceCount;
constexpr NSUInteger kFramesInFlight = 3;

struct BattlefieldVertex {
    vector_float4 position;
    vector_float4 normal;
};

struct BattlefieldInstanceGPU {
    vector_float4 centerAndMaterial;
    vector_float4 halfExtentsAndKind;
    vector_float4 tintAndRoughness;
};

struct BattlefieldSceneGPU {
    matrix_float4x4 viewProjection;
    vector_float4 cameraAndTime;
    vector_float4 sunAndFog;
};
static_assert(sizeof(BattlefieldVertex) == 32, "Battlefield vertex ABI changed");
static_assert(sizeof(BattlefieldInstanceGPU) == 48, "Battlefield instance ABI changed");

matrix_float4x4 Perspective(float verticalFOV, float aspect, float nearZ, float farZ) {
    const float ys = 1.0f / std::tan(verticalFOV * 0.5f);
    const float xs = ys / std::max(aspect, 0.1f);
    const float zs = farZ / (nearZ - farZ);
    return (matrix_float4x4){
        (vector_float4){xs, 0, 0, 0}, (vector_float4){0, ys, 0, 0},
        (vector_float4){0, 0, zs, -1}, (vector_float4){0, 0, nearZ * zs, 0}
    };
}

matrix_float4x4 View(vector_float3 camera, float yaw, float pitch, float roll) {
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    vector_float3 forward = simd_normalize((vector_float3){sy * cp, sp, cy * cp});
    vector_float3 baseRight = simd_normalize((vector_float3){cy, 0, -sy});
    vector_float3 baseUp = simd_normalize(simd_cross(forward, baseRight));
    const float cr = std::cos(roll), sr = std::sin(roll);
    vector_float3 right = simd_normalize(baseRight * cr + baseUp * sr);
    vector_float3 up = simd_normalize(baseUp * cr - baseRight * sr);
    return (matrix_float4x4){
        (vector_float4){right.x, up.x, -forward.x, 0},
        (vector_float4){right.y, up.y, -forward.y, 0},
        (vector_float4){right.z, up.z, -forward.z, 0},
        (vector_float4){-simd_dot(right, camera), -simd_dot(up, camera), simd_dot(forward, camera), 1}
    };
}

vector_float4 MaterialTint(uint8_t material) {
    switch (material) {
        case 0: return {0.48f, 0.45f, 0.37f, 0.88f}; // concrete
        case 1: return {0.22f, 0.24f, 0.23f, 0.48f}; // steel
        case 2: return {0.35f, 0.23f, 0.12f, 0.78f}; // wood
        case 3: return {0.52f, 0.25f, 0.14f, 0.90f}; // brick
        case 4: return {0.11f, 0.31f, 0.34f, 0.20f}; // glass
        case 5: return {0.39f, 0.31f, 0.19f, 0.98f}; // soil
        default: return {0.31f, 0.30f, 0.26f, 0.96f}; // rock
    }
}

void WriteInstance(BattlefieldInstanceGPU &output,
                   float minX, float minY, float minZ,
                   float maxX, float maxY, float maxZ,
                   uint8_t material, float kind) {
    output.centerAndMaterial = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f,
                                (minZ + maxZ) * 0.5f, (float)material};
    output.halfExtentsAndKind = {(maxX - minX) * 0.5f, (maxY - minY) * 0.5f,
                                 (maxZ - minZ) * 0.5f, kind};
    output.tintAndRoughness = MaterialTint(material);
}
} // namespace

@interface METSEBattlefieldRenderer ()
@property(nonatomic, readwrite, getter=isReady) BOOL ready;
@property(nonatomic, readwrite, copy) NSString *status;
@end

@implementation METSEBattlefieldRenderer {
    id<MTLRenderPipelineState> _pipeline;
    id<MTLDepthStencilState> _depthState;
    id<MTLBuffer> _vertexBuffer;
    id<MTLBuffer> _indexBuffer;
    NSArray<id<MTLBuffer>> *_instanceBuffers;
    NSUInteger _frameIndex;
    dispatch_semaphore_t _frameSemaphore;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
                        library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat
               depthPixelFormat:(MTLPixelFormat)depthPixelFormat {
    self = [super init];
    if (!self) return nil;
    _status = @"battlefield unavailable";
    _frameSemaphore = dispatch_semaphore_create(kFramesInFlight);

    static const BattlefieldVertex vertices[] = {
        {{-1,-1, 1,1},{ 0, 0, 1,0}}, {{ 1,-1, 1,1},{ 0, 0, 1,0}}, {{ 1, 1, 1,1},{ 0, 0, 1,0}}, {{-1, 1, 1,1},{ 0, 0, 1,0}},
        {{ 1,-1,-1,1},{ 0, 0,-1,0}}, {{-1,-1,-1,1},{ 0, 0,-1,0}}, {{-1, 1,-1,1},{ 0, 0,-1,0}}, {{ 1, 1,-1,1},{ 0, 0,-1,0}},
        {{-1,-1,-1,1},{-1, 0, 0,0}}, {{-1,-1, 1,1},{-1, 0, 0,0}}, {{-1, 1, 1,1},{-1, 0, 0,0}}, {{-1, 1,-1,1},{-1, 0, 0,0}},
        {{ 1,-1, 1,1},{ 1, 0, 0,0}}, {{ 1,-1,-1,1},{ 1, 0, 0,0}}, {{ 1, 1,-1,1},{ 1, 0, 0,0}}, {{ 1, 1, 1,1},{ 1, 0, 0,0}},
        {{-1, 1, 1,1},{ 0, 1, 0,0}}, {{ 1, 1, 1,1},{ 0, 1, 0,0}}, {{ 1, 1,-1,1},{ 0, 1, 0,0}}, {{-1, 1,-1,1},{ 0, 1, 0,0}},
        {{-1,-1,-1,1},{ 0,-1, 0,0}}, {{ 1,-1,-1,1},{ 0,-1, 0,0}}, {{ 1,-1, 1,1},{ 0,-1, 0,0}}, {{-1,-1, 1,1},{ 0,-1, 0,0}}
    };
    static const uint16_t indices[] = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };
    _vertexBuffer = [device newBufferWithBytes:vertices length:sizeof(vertices)
                                       options:MTLResourceStorageModeShared];
    _indexBuffer = [device newBufferWithBytes:indices length:sizeof(indices)
                                      options:MTLResourceStorageModeShared];
    NSMutableArray<id<MTLBuffer>> *instanceBuffers = [NSMutableArray arrayWithCapacity:kFramesInFlight];
    for (NSUInteger index = 0; index < kFramesInFlight; ++index) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(BattlefieldInstanceGPU) * kMaximumInstances
                                                   options:MTLResourceStorageModeShared];
        if (buffer) [instanceBuffers addObject:buffer];
    }
    _instanceBuffers = [instanceBuffers copy];

    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[1].offset = 16;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = sizeof(BattlefieldVertex);
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.label = @"METSE simulation-aligned battlefield";
    descriptor.vertexFunction = [library newFunctionWithName:@"metseBattlefieldVertex"];
    descriptor.fragmentFunction = [library newFunctionWithName:@"metseBattlefieldFragment"];
    descriptor.vertexDescriptor = vertexDescriptor;
    descriptor.colorAttachments[0].pixelFormat = colorPixelFormat;
    descriptor.depthAttachmentPixelFormat = depthPixelFormat;
    NSError *error = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!_pipeline || !_vertexBuffer || !_indexBuffer) {
        _status = [NSString stringWithFormat:@"battlefield pipeline failed: %@", error.localizedDescription];
        return self;
    }
    if (_instanceBuffers.count != kFramesInFlight) {
        _status = @"battlefield instance allocation failed";
        return self;
    }
    MTLDepthStencilDescriptor *depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionLess;
    depth.depthWriteEnabled = YES;
    _depthState = [device newDepthStencilStateWithDescriptor:depth];
    _ready = _depthState != nil;
    _status = _ready ? @"native battlefield ready" : @"battlefield depth state failed";
    return self;
}

- (void)encodeWithEncoder:(id<MTLRenderCommandEncoder>)encoder
            commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                 viewSize:(CGSize)viewSize
                obstacles:(const METSEBattlefieldObstacle *)obstacles
                    count:(NSUInteger)count
                     minX:(float)minX maxX:(float)maxX minZ:(float)minZ maxZ:(float)maxZ
                  cameraX:(float)cameraX cameraY:(float)cameraY cameraZ:(float)cameraZ
                      yaw:(float)yaw pitch:(float)pitch roll:(float)roll
                 adsAlpha:(float)adsAlpha
        simulationSeconds:(float)simulationSeconds {
    if (!self.ready || !commandBuffer || !obstacles || viewSize.width <= 0 || viewSize.height <= 0) return;
    count = std::min(count, kMaximumObstacleInstances);
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_t completionSemaphore = _frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completed) {
        (void)completed;
        dispatch_semaphore_signal(completionSemaphore);
    }];
    _frameIndex = (_frameIndex + 1) % kFramesInFlight;
    id<MTLBuffer> instances = _instanceBuffers[_frameIndex];
    auto *output = static_cast<BattlefieldInstanceGPU *>(instances.contents);
    NSUInteger write = 0;
    WriteInstance(output[write++], minX, -0.10f, minZ, maxX, 0.0f, maxZ, 5, 0.0f);
    for (NSUInteger index = 0; index < count; ++index) {
        const auto &obstacle = obstacles[index];
        WriteInstance(output[write++], obstacle.minX, obstacle.minY, obstacle.minZ,
                      obstacle.maxX, obstacle.maxY, obstacle.maxZ, obstacle.material, 1.0f);
    }
    constexpr float boundaryHeight = 0.42f;
    constexpr float boundaryThickness = 0.12f;
    WriteInstance(output[write++], minX, 0, minZ, minX + boundaryThickness, boundaryHeight, maxZ, 6, 2.0f);
    WriteInstance(output[write++], maxX - boundaryThickness, 0, minZ, maxX, boundaryHeight, maxZ, 6, 2.0f);
    WriteInstance(output[write++], minX, 0, minZ, maxX, boundaryHeight, minZ + boundaryThickness, 6, 2.0f);
    WriteInstance(output[write++], minX, 0, maxZ - boundaryThickness, maxX, boundaryHeight, maxZ, 6, 2.0f);

    const vector_float3 camera = {cameraX, cameraY, cameraZ};
    const float aspect = (float)(viewSize.width / viewSize.height);
    const float ads = std::clamp(adsAlpha, 0.0f, 1.0f);
    const float fov = 2.0f * std::atan(0.58f + (0.42f - 0.58f) * ads);
    BattlefieldSceneGPU scene{};
    scene.viewProjection = simd_mul(Perspective(fov, aspect, 0.035f, 160.0f), View(camera, yaw, pitch, roll));
    scene.cameraAndTime = {cameraX, cameraY, cameraZ, simulationSeconds};
    scene.sunAndFog = {-0.35f, 0.78f, 0.46f, 0.0175f};

    [encoder pushDebugGroup:@"Simulation-aligned battlefield"];
    [encoder setRenderPipelineState:_pipeline];
    [encoder setDepthStencilState:_depthState];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBytes:&scene length:sizeof(scene) atIndex:1];
    [encoder setVertexBuffer:instances offset:0 atIndex:2];
    [encoder setFragmentBytes:&scene length:sizeof(scene) atIndex:1];
    [encoder setFragmentBuffer:instances offset:0 atIndex:2];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:36
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_indexBuffer
                 indexBufferOffset:0
                     instanceCount:write];
    [encoder popDebugGroup];
}

@end
