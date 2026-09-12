#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef struct {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    uint8_t material;
} METSEBattlefieldObstacle;

/// Draws the simulation-owned battlefield bounds as native indexed Metal geometry.
@interface METSEBattlefieldRenderer : NSObject
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
                obstacles:(const METSEBattlefieldObstacle *)obstacles
                    count:(NSUInteger)count
                     minX:(float)minX
                     maxX:(float)maxX
                     minZ:(float)minZ
                     maxZ:(float)maxZ
                  cameraX:(float)cameraX
                  cameraY:(float)cameraY
                  cameraZ:(float)cameraZ
                      yaw:(float)yaw
                    pitch:(float)pitch
                     roll:(float)roll
                 adsAlpha:(float)adsAlpha
        simulationSeconds:(float)simulationSeconds;
@end

NS_ASSUME_NONNULL_END
