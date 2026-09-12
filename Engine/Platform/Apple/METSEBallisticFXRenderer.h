#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef struct {
    float x, y, z;
    float velocityX, velocityY, velocityZ;
    float size;
    float life01;
    uint8_t kind;
    uint8_t material;
} METSEBallisticFXRenderState;

/// World-space, depth-tested tracers and impact/fire effects.
@interface METSEBallisticFXRenderer : NSObject
@property(nonatomic, readonly, getter=isReady) BOOL ready;
@property(nonatomic, readonly, copy) NSString *status;
- (instancetype)initWithDevice:(id<MTLDevice>)device
                        library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat
               depthPixelFormat:(MTLPixelFormat)depthPixelFormat NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (void)encodeWithEncoder:(id<MTLRenderCommandEncoder>)encoder
            commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                 viewSize:(CGSize)viewSize
                instances:(const METSEBallisticFXRenderState *)instances
                    count:(NSUInteger)count
                  cameraX:(float)cameraX cameraY:(float)cameraY cameraZ:(float)cameraZ
                      yaw:(float)yaw pitch:(float)pitch roll:(float)roll
                 adsAlpha:(float)adsAlpha
        simulationSeconds:(float)simulationSeconds;
@end

NS_ASSUME_NONNULL_END
