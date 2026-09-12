#import "METSEViewmodelRenderer.h"
#import <ModelIO/ModelIO.h>
#import <simd/simd.h>
#include <algorithm>
#include <cmath>

struct METSEViewmodelUniforms {
    matrix_float4x4 modelViewProjection;
    matrix_float4x4 model;
    vector_float4 baseColorMetallic;
    vector_float4 lightAndObstruction;
};
static_assert(sizeof(METSEViewmodelUniforms) % 16 == 0,
              "Viewmodel uniforms must remain 16-byte aligned");

static matrix_float4x4 METSETranslation(float x, float y, float z) {
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[3] = (vector_float4){x, y, z, 1.0f};
    return m;
}

static matrix_float4x4 METSEScale(float value) {
    matrix_float4x4 m = matrix_identity_float4x4;
    m.columns[0].x = value;
    m.columns[1].y = value;
    m.columns[2].z = value;
    return m;
}

static matrix_float4x4 METSERotation(float x, float y, float z) {
    const float cx = std::cos(x), sx = std::sin(x);
    const float cy = std::cos(y), sy = std::sin(y);
    const float cz = std::cos(z), sz = std::sin(z);
    const matrix_float4x4 rx = {
        (vector_float4){1, 0, 0, 0}, (vector_float4){0, cx, sx, 0},
        (vector_float4){0, -sx, cx, 0}, (vector_float4){0, 0, 0, 1}
    };
    const matrix_float4x4 ry = {
        (vector_float4){cy, 0, -sy, 0}, (vector_float4){0, 1, 0, 0},
        (vector_float4){sy, 0, cy, 0}, (vector_float4){0, 0, 0, 1}
    };
    const matrix_float4x4 rz = {
        (vector_float4){cz, sz, 0, 0}, (vector_float4){-sz, cz, 0, 0},
        (vector_float4){0, 0, 1, 0}, (vector_float4){0, 0, 0, 1}
    };
    return simd_mul(rz, simd_mul(ry, rx));
}

static matrix_float4x4 METSEPerspective(float verticalFOV, float aspect, float nearZ, float farZ) {
    const float ys = 1.0f / std::tan(verticalFOV * 0.5f);
    const float xs = ys / std::max(aspect, 0.1f);
    const float zs = farZ / (nearZ - farZ);
    return (matrix_float4x4){
        (vector_float4){xs, 0, 0, 0},
        (vector_float4){0, ys, 0, 0},
        (vector_float4){0, 0, zs, -1},
        (vector_float4){0, 0, nearZ * zs, 0}
    };
}

static NSValue *METSEPaletteValue(float r, float g, float b, float metallic) {
    vector_float4 value = {r, g, b, metallic};
    return [NSValue valueWithBytes:&value objCType:@encode(vector_float4)];
}

static vector_float4 METSEColorForMaterial(NSString *name) {
    NSDictionary<NSString *, NSValue *> *palette = @{
        @"gunmetal": METSEPaletteValue(0.075f, 0.085f, 0.080f, 0.82f),
        @"receiver": METSEPaletteValue(0.105f, 0.115f, 0.105f, 0.76f),
        @"control": METSEPaletteValue(0.12f, 0.13f, 0.12f, 0.68f),
        @"magazine": METSEPaletteValue(0.10f, 0.11f, 0.095f, 0.72f),
        @"polymer": METSEPaletteValue(0.055f, 0.060f, 0.050f, 0.20f),
        @"rubber": METSEPaletteValue(0.025f, 0.028f, 0.025f, 0.08f),
        @"handguard": METSEPaletteValue(0.115f, 0.105f, 0.080f, 0.44f),
        @"rail": METSEPaletteValue(0.06f, 0.065f, 0.06f, 0.72f),
        @"edge": METSEPaletteValue(0.16f, 0.17f, 0.15f, 0.88f),
        @"barrel": METSEPaletteValue(0.08f, 0.085f, 0.08f, 0.90f),
        @"suppressor": METSEPaletteValue(0.12f, 0.115f, 0.095f, 0.58f),
        @"optic": METSEPaletteValue(0.055f, 0.06f, 0.055f, 0.78f),
        @"glass": METSEPaletteValue(0.025f, 0.24f, 0.22f, 0.18f),
        @"sleeve": METSEPaletteValue(0.18f, 0.20f, 0.13f, 0.03f),
        @"glove": METSEPaletteValue(0.14f, 0.13f, 0.10f, 0.10f)
    };
    vector_float4 color = {0.1f, 0.105f, 0.095f, 0.5f};
    NSValue *value = palette[name.lowercaseString];
    if (value) [value getValue:&color size:sizeof(color)];
    return color;
}

@interface METSEViewmodelRenderer ()
@property(nonatomic, readwrite, getter=isReady) BOOL ready;
@property(nonatomic, readwrite, copy) NSString *assetStatus;
@end

@implementation METSEViewmodelRenderer {
    NSArray<MTKMesh *> *_meshes;
    NSArray<NSArray<NSString *> *> *_materialNames;
    id<MTLRenderPipelineState> _pipeline;
    id<MTLDepthStencilState> _depthState;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
                        library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat
               depthPixelFormat:(MTLPixelFormat)depthPixelFormat
                         bundle:(NSBundle *)bundle {
    self = [super init];
    if (!self) return nil;
    _assetStatus = @"viewmodel unavailable";

    MDLVertexDescriptor *vertexDescriptor = [MDLVertexDescriptor new];
    vertexDescriptor.attributes[0] = [[MDLVertexAttribute alloc]
        initWithName:MDLVertexAttributePosition format:MDLVertexFormatFloat3 offset:0 bufferIndex:0];
    vertexDescriptor.attributes[1] = [[MDLVertexAttribute alloc]
        initWithName:MDLVertexAttributeNormal format:MDLVertexFormatFloat3 offset:12 bufferIndex:0];
    vertexDescriptor.layouts[0] = [[MDLVertexBufferLayout alloc] initWithStride:24];

    MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
    pipelineDescriptor.label = @"METSE native viewmodel";
    pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"metseMeshVertex"];
    pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"metseMeshFragment"];
    pipelineDescriptor.vertexDescriptor = MTKMetalVertexDescriptorFromModelIO(vertexDescriptor);
    pipelineDescriptor.colorAttachments[0].pixelFormat = colorPixelFormat;
    pipelineDescriptor.depthAttachmentPixelFormat = depthPixelFormat;
    NSError *pipelineError = nil;
    _pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&pipelineError];
    if (!_pipeline) {
        _assetStatus = [NSString stringWithFormat:@"mesh pipeline failed: %@", pipelineError.localizedDescription];
        return self;
    }

    MTLDepthStencilDescriptor *depthDescriptor = [MTLDepthStencilDescriptor new];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
    depthDescriptor.depthWriteEnabled = YES;
    _depthState = [device newDepthStencilStateWithDescriptor:depthDescriptor];

    NSURL *assetURL = [bundle URLForResource:@"viewmodel" withExtension:@"obj"
                                subdirectory:@"Assets/Weapons/M4A1"];
    if (!assetURL) {
        for (NSURL *candidate in [bundle URLsForResourcesWithExtension:@"obj" subdirectory:nil]) {
            if ([candidate.lastPathComponent isEqualToString:@"viewmodel.obj"]) { assetURL = candidate; break; }
        }
    }
    if (!assetURL) return self;

    MTKMeshBufferAllocator *allocator = [[MTKMeshBufferAllocator alloc] initWithDevice:device];
    MDLAsset *asset = [[MDLAsset alloc] initWithURL:assetURL
                                  vertexDescriptor:vertexDescriptor
                                   bufferAllocator:allocator];
    NSArray<MDLMesh *> *sourceMeshes = nil;
    NSError *meshError = nil;
    _meshes = [MTKMesh newMeshesFromAsset:asset device:device sourceMeshes:&sourceMeshes error:&meshError];
    if (!_meshes.count || meshError) {
        _assetStatus = [NSString stringWithFormat:@"viewmodel load failed: %@", meshError.localizedDescription];
        return self;
    }

    NSMutableArray<NSArray<NSString *> *> *allNames = [NSMutableArray arrayWithCapacity:sourceMeshes.count];
    for (MDLMesh *mesh in sourceMeshes) {
        NSMutableArray<NSString *> *names = [NSMutableArray array];
        for (MDLSubmesh *submesh in mesh.submeshes) {
            NSString *name = submesh.material.name.length ? submesh.material.name : submesh.name;
            [names addObject:name.length ? name : @"receiver"];
        }
        [allNames addObject:names];
    }
    _materialNames = allNames;
    _ready = YES;
    NSUInteger totalVertices = 0;
    for (MTKMesh *mesh in _meshes) totalVertices += mesh.vertexCount;
    _assetStatus = [NSString stringWithFormat:@"native mesh ready (%lu mesh, %lu vertices)",
                    (unsigned long)_meshes.count,
                    (unsigned long)totalVertices];
    return self;
}

- (void)encodeWithEncoder:(id<MTLRenderCommandEncoder>)encoder
                 viewSize:(CGSize)viewSize
                  adsAlpha:(float)adsAlpha
                    swayX:(float)swayX
                    swayY:(float)swayY
              recoilPitch:(float)recoilPitch
                recoilYaw:(float)recoilYaw
               cameraRoll:(float)cameraRoll
              obstructed:(BOOL)obstructed
          reloadRemaining:(float)reloadRemaining
         simulationSeconds:(float)simulationSeconds {
    if (!self.ready || viewSize.width <= 0 || viewSize.height <= 0) return;
    const float ads = std::clamp(adsAlpha, 0.0f, 1.0f);
    const float reload = std::clamp(reloadRemaining, 0.0f, 1.0f);
    const float obstruction = obstructed ? 1.0f : 0.0f;
    const float breathing = std::sin(simulationSeconds * 1.7f) * (1.0f - ads) * 0.0025f;
    const float x = (0.225f * (1.0f - ads)) + swayX * 5.5f + recoilYaw * 0.95f;
    const float y = (-0.205f + ads * 0.052f) + swayY * 5.0f - recoilPitch * 0.75f + breathing - obstruction * 0.12f;
    const float z = -0.68f - obstruction * 0.08f;
    const float pitch = -0.035f - recoilPitch * 1.4f + reload * 0.42f + obstruction * 0.38f;
    const float yaw = -0.065f * (1.0f - ads) + recoilYaw * 1.35f + reload * 0.18f;
    const float roll = -0.035f * (1.0f - ads) + cameraRoll * 0.32f + reload * 0.55f;

    matrix_float4x4 model = simd_mul(METSETranslation(x, y, z),
        simd_mul(METSERotation(pitch, yaw, roll), METSEScale(0.50f)));
    const float aspect = (float)(viewSize.width / viewSize.height);
    const float fov = (62.0f - ads * 10.0f) * 3.14159265358979323846f / 180.0f;
    matrix_float4x4 projection = METSEPerspective(fov, aspect, 0.025f, 8.0f);

    [encoder setRenderPipelineState:_pipeline];
    [encoder setDepthStencilState:_depthState];
    [encoder setCullMode:MTLCullModeNone];
    for (NSUInteger meshIndex = 0; meshIndex < _meshes.count; ++meshIndex) {
        MTKMesh *mesh = _meshes[meshIndex];
        for (NSUInteger bufferIndex = 0; bufferIndex < mesh.vertexBuffers.count; ++bufferIndex) {
            MTKMeshBuffer *buffer = mesh.vertexBuffers[bufferIndex];
            [encoder setVertexBuffer:buffer.buffer offset:buffer.offset atIndex:bufferIndex];
        }
        for (NSUInteger submeshIndex = 0; submeshIndex < mesh.submeshes.count; ++submeshIndex) {
            NSString *materialName = submeshIndex < _materialNames[meshIndex].count
                ? _materialNames[meshIndex][submeshIndex] : @"receiver";
            METSEViewmodelUniforms uniforms{};
            uniforms.model = model;
            uniforms.modelViewProjection = simd_mul(projection, model);
            uniforms.baseColorMetallic = METSEColorForMaterial(materialName);
            uniforms.lightAndObstruction = (vector_float4){-0.35f, 0.78f, 0.52f, obstruction};
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            MTKSubmesh *submesh = mesh.submeshes[submeshIndex];
            [encoder drawIndexedPrimitives:submesh.primitiveType
                                indexCount:submesh.indexCount
                                 indexType:submesh.indexType
                               indexBuffer:submesh.indexBuffer.buffer
                         indexBufferOffset:submesh.indexBuffer.offset];
        }
    }
}

@end
