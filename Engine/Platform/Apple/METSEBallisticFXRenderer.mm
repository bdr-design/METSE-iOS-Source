#import "METSEBallisticFXRenderer.h"
#import <dispatch/dispatch.h>
#import <simd/simd.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr NSUInteger kMaximumProjectiles=128;
constexpr NSUInteger kMaximumFX=48;
constexpr NSUInteger kMaximumInstances=kMaximumProjectiles+kMaximumFX;
constexpr NSUInteger kFramesInFlight=3;
struct Vertex { vector_float4 position; vector_float4 normal; };
struct InstanceGPU { vector_float4 positionAndKind; vector_float4 directionAndLength; vector_float4 materialSizeLife; };
struct SceneGPU { matrix_float4x4 viewProjection; vector_float4 cameraAndTime; vector_float4 sunAndFog; };
static_assert(sizeof(Vertex)==32,"Ballistic FX vertex ABI changed");
static_assert(sizeof(InstanceGPU)==48,"Ballistic FX instance ABI changed");

matrix_float4x4 Perspective(float fov,float aspect,float nearZ,float farZ){
    const float ys=1.0f/std::tan(fov*.5f),xs=ys/std::max(aspect,.1f),zs=farZ/(nearZ-farZ);
    return (matrix_float4x4){(vector_float4){xs,0,0,0},(vector_float4){0,ys,0,0},
        (vector_float4){0,0,zs,-1},(vector_float4){0,0,nearZ*zs,0}};
}
matrix_float4x4 View(vector_float3 camera,float yaw,float pitch,float roll){
    const float cp=std::cos(pitch),sp=std::sin(pitch),sy=std::sin(yaw),cy=std::cos(yaw);
    const vector_float3 forward=simd_normalize((vector_float3){sy*cp,sp,cy*cp});
    const vector_float3 baseRight=simd_normalize((vector_float3){cy,0,-sy});
    const vector_float3 baseUp=simd_normalize(simd_cross(forward,baseRight));
    const float cr=std::cos(roll),sr=std::sin(roll);
    const vector_float3 right=simd_normalize(baseRight*cr+baseUp*sr),up=simd_normalize(baseUp*cr-baseRight*sr);
    return (matrix_float4x4){(vector_float4){right.x,up.x,-forward.x,0},(vector_float4){right.y,up.y,-forward.y,0},
        (vector_float4){right.z,up.z,-forward.z,0},(vector_float4){-simd_dot(right,camera),-simd_dot(up,camera),simd_dot(forward,camera),1}};
}
} // namespace

@interface METSEBallisticFXRenderer ()
@property(nonatomic,readwrite,getter=isReady) BOOL ready;
@property(nonatomic,readwrite,copy) NSString *status;
@end

@implementation METSEBallisticFXRenderer {
    id<MTLRenderPipelineState> _pipeline; id<MTLDepthStencilState> _depthState;
    id<MTLBuffer> _vertexBuffer,_indexBuffer; NSArray<id<MTLBuffer>> *_instanceBuffers;
    NSUInteger _frameIndex; dispatch_semaphore_t _frameSemaphore;
}
- (instancetype)initWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat depthPixelFormat:(MTLPixelFormat)depthPixelFormat {
    self=[super init]; if(!self)return nil; _status=@"ballistic FX unavailable"; _frameSemaphore=dispatch_semaphore_create(kFramesInFlight);
    static const Vertex vertices[]={
        {{-1,-1, 1,1},{0,0,1,0}},{{ 1,-1, 1,1},{0,0,1,0}},{{ 1, 1, 1,1},{0,0,1,0}},{{-1, 1, 1,1},{0,0,1,0}},
        {{ 1,-1,-1,1},{0,0,-1,0}},{{-1,-1,-1,1},{0,0,-1,0}},{{-1, 1,-1,1},{0,0,-1,0}},{{ 1, 1,-1,1},{0,0,-1,0}},
        {{-1,-1,-1,1},{-1,0,0,0}},{{-1,-1, 1,1},{-1,0,0,0}},{{-1, 1, 1,1},{-1,0,0,0}},{{-1, 1,-1,1},{-1,0,0,0}},
        {{ 1,-1, 1,1},{1,0,0,0}},{{ 1,-1,-1,1},{1,0,0,0}},{{ 1, 1,-1,1},{1,0,0,0}},{{ 1, 1, 1,1},{1,0,0,0}},
        {{-1, 1, 1,1},{0,1,0,0}},{{ 1, 1, 1,1},{0,1,0,0}},{{ 1, 1,-1,1},{0,1,0,0}},{{-1, 1,-1,1},{0,1,0,0}},
        {{-1,-1,-1,1},{0,-1,0,0}},{{ 1,-1,-1,1},{0,-1,0,0}},{{ 1,-1, 1,1},{0,-1,0,0}},{{-1,-1, 1,1},{0,-1,0,0}}};
    static const uint16_t indices[]={0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11,12,13,14,12,14,15,16,17,18,16,18,19,20,21,22,20,22,23};
    _vertexBuffer=[device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];
    _indexBuffer=[device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
    NSMutableArray<id<MTLBuffer>> *buffers=[NSMutableArray arrayWithCapacity:kFramesInFlight];
    for(NSUInteger i=0;i<kFramesInFlight;++i){id<MTLBuffer>b=[device newBufferWithLength:sizeof(InstanceGPU)*kMaximumInstances options:MTLResourceStorageModeShared];if(b)[buffers addObject:b];}
    _instanceBuffers=[buffers copy];
    MTLVertexDescriptor *vd=[MTLVertexDescriptor vertexDescriptor]; vd.attributes[0].format=MTLVertexFormatFloat3;vd.attributes[0].offset=0;vd.attributes[0].bufferIndex=0;
    vd.attributes[1].format=MTLVertexFormatFloat3;vd.attributes[1].offset=16;vd.attributes[1].bufferIndex=0;vd.layouts[0].stride=sizeof(Vertex);vd.layouts[0].stepFunction=MTLVertexStepFunctionPerVertex;
    MTLRenderPipelineDescriptor *pd=[MTLRenderPipelineDescriptor new];pd.label=@"METSE native ballistic FX";
    pd.vertexFunction=[library newFunctionWithName:@"metseBallisticFXVertex"];pd.fragmentFunction=[library newFunctionWithName:@"metseBallisticFXFragment"];
    pd.vertexDescriptor=vd;pd.colorAttachments[0].pixelFormat=colorPixelFormat;pd.depthAttachmentPixelFormat=depthPixelFormat;
    NSError *error=nil;_pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!_pipeline||!_vertexBuffer||!_indexBuffer||_instanceBuffers.count!=kFramesInFlight){_status=[NSString stringWithFormat:@"ballistic FX pipeline failed: %@",error.localizedDescription];return self;}
    MTLDepthStencilDescriptor *depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionLess;depth.depthWriteEnabled=YES;
    _depthState=[device newDepthStencilStateWithDescriptor:depth];_ready=_depthState!=nil;_status=_ready?@"native ballistic FX ready":@"ballistic FX depth failed";return self;
}
- (void)encodeWithEncoder:(id<MTLRenderCommandEncoder>)encoder commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                 viewSize:(CGSize)viewSize instances:(const METSEBallisticFXRenderState *)states count:(NSUInteger)count
                  cameraX:(float)cameraX cameraY:(float)cameraY cameraZ:(float)cameraZ yaw:(float)yaw pitch:(float)pitch roll:(float)roll
                 adsAlpha:(float)adsAlpha simulationSeconds:(float)simulationSeconds {
    if(!self.ready||!commandBuffer||!states||viewSize.width<=0||viewSize.height<=0)return;
    count=std::min(count,kMaximumInstances);dispatch_semaphore_wait(_frameSemaphore,DISPATCH_TIME_FOREVER);dispatch_semaphore_t sem=_frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completed){(void)completed;dispatch_semaphore_signal(sem);}];
    _frameIndex=(_frameIndex+1)%kFramesInFlight;id<MTLBuffer>buffer=_instanceBuffers[_frameIndex];auto*out=static_cast<InstanceGPU*>(buffer.contents);
    for(NSUInteger i=0;i<count;++i){const auto&s=states[i];vector_float3 velocity=(vector_float3){s.velocityX,s.velocityY,s.velocityZ};float speed=simd_length(velocity);
        vector_float3 direction=speed>.001f?velocity/speed:(vector_float3){0,1,0};float length=s.kind==0?std::clamp(speed*.0015f,.28f,1.45f):s.size;
        out[i]={{s.x,s.y,s.z,(float)s.kind},{direction.x,direction.y,direction.z,length},{(float)s.material,s.size,s.life01,0}};}
    if(count==0)return;SceneGPU scene{};vector_float3 camera=(vector_float3){cameraX,cameraY,cameraZ};float fov=2*std::atan(.58f+(.42f-.58f)*std::clamp(adsAlpha,0.f,1.f));
    scene.viewProjection=simd_mul(Perspective(fov,(float)(viewSize.width/viewSize.height),.035f,160.f),View(camera,yaw,pitch,roll));
    scene.cameraAndTime={cameraX,cameraY,cameraZ,simulationSeconds};scene.sunAndFog={-.35f,.78f,.46f,.0175f};
    [encoder pushDebugGroup:@"Native ballistic FX"];[encoder setRenderPipelineState:_pipeline];[encoder setDepthStencilState:_depthState];[encoder setCullMode:MTLCullModeBack];
    [encoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0];[encoder setVertexBytes:&scene length:sizeof(scene) atIndex:1];[encoder setVertexBuffer:buffer offset:0 atIndex:2];
    [encoder setFragmentBytes:&scene length:sizeof(scene) atIndex:1];[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:36 indexType:MTLIndexTypeUInt16 indexBuffer:_indexBuffer indexBufferOffset:0 instanceCount:count];[encoder popDebugGroup];
}
@end
