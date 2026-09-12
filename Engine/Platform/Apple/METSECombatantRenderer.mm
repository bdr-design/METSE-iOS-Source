#import "METSECombatantRenderer.h"
#import <dispatch/dispatch.h>
#import <simd/simd.h>
#import <ModelIO/ModelIO.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr NSUInteger kMaximumCombatants = 32;
constexpr NSUInteger kFramesInFlight = 3;
constexpr float kPi = 3.14159265358979323846f;

struct CombatantVertex {
    vector_float4 positionAndPart;
    vector_float4 normal;
};
struct CombatantInstanceGPU {
    vector_float4 positionAndFacing;
    vector_float4 healthTierStateLOS;
    vector_float4 actionIdentityPhase;
};
struct CombatantSceneGPU {
    matrix_float4x4 viewProjection;
    vector_float4 cameraAndTime;
    vector_float4 sunAndFog;
};
static_assert(sizeof(CombatantVertex) == 32, "Combatant vertex ABI changed");
static_assert(sizeof(CombatantInstanceGPU) == 48, "Combatant instance ABI changed");

matrix_float4x4 Perspective(float verticalFOV, float aspect, float nearZ, float farZ) {
    const float ys = 1.0f / std::tan(verticalFOV * 0.5f);
    const float xs = ys / std::max(aspect, 0.1f);
    const float zs = farZ / (nearZ - farZ);
    return (matrix_float4x4){
        (vector_float4){xs,0,0,0}, (vector_float4){0,ys,0,0},
        (vector_float4){0,0,zs,-1}, (vector_float4){0,0,nearZ*zs,0}
    };
}

matrix_float4x4 View(vector_float3 camera, float yaw, float pitch, float roll) {
    const float cp=std::cos(pitch), sp=std::sin(pitch), sy=std::sin(yaw), cy=std::cos(yaw);
    const vector_float3 forward=simd_normalize((vector_float3){sy*cp,sp,cy*cp});
    const vector_float3 baseRight=simd_normalize((vector_float3){cy,0,-sy});
    const vector_float3 baseUp=simd_normalize(simd_cross(forward,baseRight));
    const float cr=std::cos(roll), sr=std::sin(roll);
    const vector_float3 right=simd_normalize(baseRight*cr+baseUp*sr);
    const vector_float3 up=simd_normalize(baseUp*cr-baseRight*sr);
    return (matrix_float4x4){
        (vector_float4){right.x,up.x,-forward.x,0},
        (vector_float4){right.y,up.y,-forward.y,0},
        (vector_float4){right.z,up.z,-forward.z,0},
        (vector_float4){-simd_dot(right,camera),-simd_dot(up,camera),simd_dot(forward,camera),1}
    };
}

void AddVertex(std::vector<CombatantVertex>& vertices, vector_float3 p, vector_float3 n, float part) {
    vertices.push_back({{p.x,p.y,p.z,part},{n.x,n.y,n.z,0}});
}
vector_float3 V(float x,float y,float z) { return (vector_float3){x,y,z}; }

void AddBox(std::vector<CombatantVertex>& vertices, std::vector<uint16_t>& indices,
            vector_float3 center, vector_float3 extent, float part) {
    static const vector_float3 normals[6]={{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    static const int corners[6][4]={{4,5,7,6},{1,0,2,3},{0,4,6,2},{5,1,3,7},{2,6,7,3},{0,1,5,4}};
    vector_float3 p[8];
    for(int i=0;i<8;++i) p[i]=center+(vector_float3){(i&1)?extent.x:-extent.x,(i&2)?extent.y:-extent.y,(i&4)?extent.z:-extent.z};
    for(int face=0;face<6;++face){
        const uint16_t base=(uint16_t)vertices.size();
        for(int corner=0;corner<4;++corner) AddVertex(vertices,p[corners[face][corner]],normals[face],part);
        indices.insert(indices.end(),{base,(uint16_t)(base+1),(uint16_t)(base+2),base,(uint16_t)(base+2),(uint16_t)(base+3)});
    }
}

void AddEllipsoid(std::vector<CombatantVertex>& vertices, std::vector<uint16_t>& indices,
                  vector_float3 center, vector_float3 radii, float part) {
    constexpr int rings=7, segments=12;
    const uint16_t base=(uint16_t)vertices.size();
    for(int ring=0;ring<=rings;++ring){
        const float v=(float)ring/(float)rings, phi=v*kPi;
        for(int segment=0;segment<=segments;++segment){
            const float u=(float)segment/(float)segments, theta=u*2.0f*kPi;
            const vector_float3 unit={std::sin(phi)*std::cos(theta),std::cos(phi),std::sin(phi)*std::sin(theta)};
            const vector_float3 p=center+unit*radii;
            const vector_float3 n=simd_normalize((vector_float3){unit.x/radii.x,unit.y/radii.y,unit.z/radii.z});
            AddVertex(vertices,p,n,part);
        }
    }
    for(int ring=0;ring<rings;++ring) for(int segment=0;segment<segments;++segment){
        const uint16_t a=base+(uint16_t)(ring*(segments+1)+segment), b=(uint16_t)(a+segments+1);
        indices.insert(indices.end(),{a,(uint16_t)(a+1),b,(uint16_t)(a+1),(uint16_t)(b+1),b});
    }
}

void AddCylinder(std::vector<CombatantVertex>& vertices, std::vector<uint16_t>& indices,
                 vector_float3 a, vector_float3 b, float radius, float part) {
    constexpr int segments=10;
    const vector_float3 axis=simd_normalize(b-a);
    const vector_float3 reference=std::abs(axis.y)<0.92f?(vector_float3){0,1,0}:(vector_float3){1,0,0};
    const vector_float3 side=simd_normalize(simd_cross(axis,reference));
    const vector_float3 up=simd_normalize(simd_cross(side,axis));
    const uint16_t base=(uint16_t)vertices.size();
    for(int end=0;end<2;++end) for(int segment=0;segment<=segments;++segment){
        const float angle=(float)segment/(float)segments*2.0f*kPi;
        const vector_float3 normal=side*std::cos(angle)+up*std::sin(angle);
        AddVertex(vertices,(end?b:a)+normal*radius,normal,part);
    }
    for(int segment=0;segment<segments;++segment){
        const uint16_t a0=base+(uint16_t)segment, b0=(uint16_t)(base+segments+1+segment);
        indices.insert(indices.end(),{a0,b0,(uint16_t)(a0+1),(uint16_t)(a0+1),b0,(uint16_t)(b0+1)});
    }
}

void BuildCombatantMesh(std::vector<CombatantVertex>& vertices, std::vector<uint16_t>& indices) {
    AddBox(vertices,indices,V(-.15f,.10f,.05f),V(.12f,.10f,.22f),5); AddBox(vertices,indices,V(.15f,.10f,.05f),V(.12f,.10f,.22f),5);
    AddCylinder(vertices,indices,V(-.15f,.18f,0),V(-.14f,.83f,0),.115f,0); AddCylinder(vertices,indices,V(.15f,.18f,0),V(.14f,.83f,0),.115f,0);
    AddBox(vertices,indices,V(0,.88f,0),V(.28f,.17f,.17f),0); AddBox(vertices,indices,V(0,1.30f,0),V(.30f,.39f,.18f),0);
    AddBox(vertices,indices,V(0,1.31f,.15f),V(.34f,.30f,.10f),1); AddBox(vertices,indices,V(0,1.31f,.265f),V(.21f,.17f,.025f),1);
    AddCylinder(vertices,indices,V(-.31f,1.52f,0),V(-.34f,1.08f,.16f),.095f,0); AddCylinder(vertices,indices,V(.31f,1.52f,0),V(.34f,1.08f,.16f),.095f,0);
    AddEllipsoid(vertices,indices,V(-.34f,1.03f,.19f),V(.09f,.09f,.09f),2); AddEllipsoid(vertices,indices,V(.34f,1.03f,.19f),V(.09f,.09f,.09f),2);
    AddEllipsoid(vertices,indices,V(0,1.82f,0),V(.145f,.18f,.145f),2); AddEllipsoid(vertices,indices,V(0,1.93f,-.005f),V(.18f,.115f,.17f),3);
    AddBox(vertices,indices,V(0,1.92f,-.13f),V(.20f,.055f,.035f),3);
    AddBox(vertices,indices,V(.11f,1.24f,.42f),V(.055f,.065f,.43f),4); AddBox(vertices,indices,V(.11f,1.27f,.43f),V(.10f,.10f,.15f),4);
    AddBox(vertices,indices,V(.11f,1.34f,.48f),V(.055f,.055f,.08f),4); AddCylinder(vertices,indices,V(.11f,1.24f,.83f),V(.11f,1.24f,1.07f),.032f,4);
}

// Build 012-A: replaces the procedural placeholder above with the healed/rigged body
// asset when it loads successfully. Per project requirement, ANY failure at ANY step
// here must fall through to the caller still holding the procedural mesh untouched -
// this function only ever REPLACES vertices/indices on full, verified success.
bool LoadCombatantBodyAsset(std::vector<CombatantVertex>& vertices, std::vector<uint16_t>& indices) {
    MDLVertexDescriptor *vertexDescriptor = [MDLVertexDescriptor new];
    vertexDescriptor.attributes[0] = [[MDLVertexAttribute alloc]
        initWithName:MDLVertexAttributePosition format:MDLVertexFormatFloat3 offset:0 bufferIndex:0];
    vertexDescriptor.attributes[1] = [[MDLVertexAttribute alloc]
        initWithName:MDLVertexAttributeNormal format:MDLVertexFormatFloat3 offset:12 bufferIndex:0];
    vertexDescriptor.layouts[0] = [[MDLVertexBufferLayout alloc] initWithStride:24];

    NSBundle *bundle = NSBundle.mainBundle;
    NSURL *assetURL = [bundle URLForResource:@"soldier_body_v1" withExtension:@"usdz"
                                subdirectory:@"Assets/Characters/Soldier"];
    if (!assetURL) {
        for (NSURL *candidate in [bundle URLsForResourcesWithExtension:@"usdz" subdirectory:nil]) {
            if ([candidate.lastPathComponent isEqualToString:@"soldier_body_v1.usdz"]) { assetURL = candidate; break; }
        }
    }
    if (!assetURL) return false;

    MDLAsset *asset = [[MDLAsset alloc] initWithURL:assetURL vertexDescriptor:vertexDescriptor bufferAllocator:nil];
    if (asset.count == 0) return false;

    NSMutableArray<MDLMesh *> *meshes = [NSMutableArray array];
    __block void (^collect)(MDLObject *);
    collect = ^(MDLObject *object) {
        if (!object) return;
        if ([object isKindOfClass:[MDLMesh class]]) [meshes addObject:(MDLMesh *)object];
        for (NSUInteger i = 0; i < object.children.count; ++i) collect([object.children objectAtIndexedSubscript:i]);
    };
    for (NSUInteger i = 0; i < asset.count; ++i) collect([asset objectAtIndex:i]);
    if (meshes.count == 0) return false;

    MDLMesh *mesh = meshes.firstObject;
    if (mesh.vertexCount == 0 || mesh.vertexBuffers.count == 0) return false;
    id<MDLMeshBuffer> vertexBuffer = mesh.vertexBuffers.firstObject;
    MDLMeshBufferMap *vertexMap = [vertexBuffer map];
    if (!vertexMap || !vertexMap.bytes) return false;
    const uint8_t *rawVertices = (const uint8_t *)vertexMap.bytes;

    std::vector<CombatantVertex> loadedVertices;
    loadedVertices.reserve(mesh.vertexCount);
    for (NSUInteger i = 0; i < mesh.vertexCount; ++i) {
        const float *p = (const float *)(rawVertices + i * 24);
        // part = 0 uniformly: the healed asset has no per-region material split yet
        // (documented in Docs/BUILD012_A_SOLDIER_BODY_PROVENANCE_AR.md).
        loadedVertices.push_back({{p[0], p[1], p[2], 0.0f}, {p[3], p[4], p[5], 0.0f}});
    }

    std::vector<uint16_t> loadedIndices;
    for (MDLSubmesh *submesh in mesh.submeshes) {
        id<MDLMeshBuffer> indexBuffer = submesh.indexBuffer;
        if (!indexBuffer) return false;
        MDLMeshBufferMap *indexMap = [indexBuffer map];
        if (!indexMap || !indexMap.bytes) return false;
        const NSUInteger indexCount = submesh.indexCount;
        if (submesh.indexType == MDLIndexBitDepthUInt16) {
            const uint16_t *idx = (const uint16_t *)indexMap.bytes;
            loadedIndices.insert(loadedIndices.end(), idx, idx + indexCount);
        } else if (submesh.indexType == MDLIndexBitDepthUInt32) {
            const uint32_t *idx = (const uint32_t *)indexMap.bytes;
            for (NSUInteger i = 0; i < indexCount; ++i) {
                if (idx[i] > UINT16_MAX) return false;
                loadedIndices.push_back((uint16_t)idx[i]);
            }
        } else {
            return false;
        }
    }
    if (loadedVertices.empty() || loadedIndices.empty()) return false;

    vertices = std::move(loadedVertices);
    indices = std::move(loadedIndices);
    return true;
}
} // namespace

@interface METSECombatantRenderer ()
@property(nonatomic, readwrite, getter=isReady) BOOL ready;
@property(nonatomic, readwrite, copy) NSString *status;
@end

@implementation METSECombatantRenderer {
    id<MTLRenderPipelineState> _pipeline;
    id<MTLDepthStencilState> _depthState;
    id<MTLBuffer> _vertexBuffer, _indexBuffer;
    NSArray<id<MTLBuffer>> *_instanceBuffers;
    NSUInteger _indexCount, _frameIndex;
    dispatch_semaphore_t _frameSemaphore;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat depthPixelFormat:(MTLPixelFormat)depthPixelFormat {
    self=[super init]; if(!self) return nil;
    _status=@"combatant geometry unavailable"; _frameSemaphore=dispatch_semaphore_create(kFramesInFlight);
    std::vector<CombatantVertex> vertices; std::vector<uint16_t> indices; BuildCombatantMesh(vertices,indices);
    const bool usedBodyAsset = LoadCombatantBodyAsset(vertices,indices);
    _vertexBuffer=[device newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(CombatantVertex) options:MTLResourceStorageModeShared];
    _indexBuffer=[device newBufferWithBytes:indices.data() length:indices.size()*sizeof(uint16_t) options:MTLResourceStorageModeShared];
    _indexCount=indices.size();
    NSMutableArray<id<MTLBuffer>> *buffers=[NSMutableArray arrayWithCapacity:kFramesInFlight];
    for(NSUInteger i=0;i<kFramesInFlight;++i){ id<MTLBuffer> b=[device newBufferWithLength:sizeof(CombatantInstanceGPU)*kMaximumCombatants options:MTLResourceStorageModeShared]; if(b)[buffers addObject:b]; }
    _instanceBuffers=[buffers copy];
    MTLVertexDescriptor *vd=[MTLVertexDescriptor vertexDescriptor];
    vd.attributes[0].format=MTLVertexFormatFloat4; vd.attributes[0].offset=0; vd.attributes[0].bufferIndex=0;
    vd.attributes[1].format=MTLVertexFormatFloat3; vd.attributes[1].offset=16; vd.attributes[1].bufferIndex=0;
    vd.layouts[0].stride=sizeof(CombatantVertex); vd.layouts[0].stepFunction=MTLVertexStepFunctionPerVertex;
    MTLRenderPipelineDescriptor *pd=[MTLRenderPipelineDescriptor new]; pd.label=@"METSE native combatants";
    pd.vertexFunction=[library newFunctionWithName:@"metseCombatantVertex"]; pd.fragmentFunction=[library newFunctionWithName:@"metseCombatantFragment"];
    pd.vertexDescriptor=vd; pd.colorAttachments[0].pixelFormat=colorPixelFormat; pd.depthAttachmentPixelFormat=depthPixelFormat;
    NSError *error=nil; _pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!_pipeline||!_vertexBuffer||!_indexBuffer||_instanceBuffers.count!=kFramesInFlight){ _status=[NSString stringWithFormat:@"combatant pipeline failed: %@",error.localizedDescription]; return self; }
    MTLDepthStencilDescriptor *depth=[MTLDepthStencilDescriptor new]; depth.depthCompareFunction=MTLCompareFunctionLess; depth.depthWriteEnabled=YES;
    _depthState=[device newDepthStencilStateWithDescriptor:depth]; _ready=_depthState!=nil;
    _status=_ready?[NSString stringWithFormat:@"%@ combatant proxy ready (%lu triangles)",usedBodyAsset?@"healed-asset":@"native procedural",(unsigned long)(_indexCount/3)]:@"combatant depth state failed";
    return self;
}

- (void)encodeWithEncoder:(id<MTLRenderCommandEncoder>)encoder commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                 viewSize:(CGSize)viewSize combatants:(const METSECombatantRenderState *)combatants count:(NSUInteger)count
                  cameraX:(float)cameraX cameraY:(float)cameraY cameraZ:(float)cameraZ
                      yaw:(float)yaw pitch:(float)pitch roll:(float)roll adsAlpha:(float)adsAlpha
        simulationSeconds:(float)simulationSeconds {
    if(!self.ready||!commandBuffer||!combatants||viewSize.width<=0||viewSize.height<=0) return;
    dispatch_semaphore_wait(_frameSemaphore,DISPATCH_TIME_FOREVER); dispatch_semaphore_t semaphore=_frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> completed){ (void)completed; dispatch_semaphore_signal(semaphore); }];
    _frameIndex=(_frameIndex+1)%kFramesInFlight; id<MTLBuffer> buffer=_instanceBuffers[_frameIndex];
    auto *out=static_cast<CombatantInstanceGPU *>(buffer.contents); NSUInteger write=0;
    for(NSUInteger i=0;i<std::min(count,kMaximumCombatants);++i){
        const auto &c=combatants[i]; if(c.visibilityTier>=3) continue;
        out[write++]={{c.x,c.y,c.z,c.facingYaw},{c.health01,(float)c.visibilityTier,(float)c.combatState,(float)c.lineOfSight},
                      {(float)c.actionState,(float)c.identity,(float)(c.identity%17u)*0.37f,0}};
    }
    if(write==0) return;
    CombatantSceneGPU scene{}; const vector_float3 camera={cameraX,cameraY,cameraZ};
    const float fov=2.0f*std::atan(0.58f+(0.42f-0.58f)*std::clamp(adsAlpha,0.0f,1.0f));
    scene.viewProjection=simd_mul(Perspective(fov,(float)(viewSize.width/viewSize.height),.035f,160.0f),View(camera,yaw,pitch,roll));
    scene.cameraAndTime={cameraX,cameraY,cameraZ,simulationSeconds}; scene.sunAndFog={-.35f,.78f,.46f,.0175f};
    [encoder pushDebugGroup:@"Simulation-owned combatants"]; [encoder setRenderPipelineState:_pipeline]; [encoder setDepthStencilState:_depthState];
    [encoder setCullMode:MTLCullModeBack]; [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0]; [encoder setVertexBytes:&scene length:sizeof(scene) atIndex:1]; [encoder setVertexBuffer:buffer offset:0 atIndex:2];
    [encoder setFragmentBytes:&scene length:sizeof(scene) atIndex:1];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_indexCount indexType:MTLIndexTypeUInt16 indexBuffer:_indexBuffer indexBufferOffset:0 instanceCount:write];
    [encoder popDebugGroup];
}
@end
