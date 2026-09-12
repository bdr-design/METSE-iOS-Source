#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef struct {
    float x, y, z;
    float facingYaw;
    float health01;
    uint8_t visibilityTier;
    uint8_t combatState;
    uint8_t actionState;
    uint8_t lineOfSight;
    uint32_t identity;
} METSECombatantRenderState;

/// Depth-tested native geometry for simulation-owned AI combatants.
@interface METSECombatantRenderer : NSObject
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
               combatants:(const METSECombatantRenderState *)combatants
                    count:(NSUInteger)count
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
