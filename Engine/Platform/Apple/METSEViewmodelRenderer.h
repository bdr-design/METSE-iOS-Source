#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

/// Presentation-only first-person mesh renderer. Simulation state remains in EngineCore.
@interface METSEViewmodelRenderer : NSObject
@property(nonatomic, readonly, getter=isReady) BOOL ready;
@property(nonatomic, readonly, copy) NSString *assetStatus;

- (instancetype)initWithDevice:(id<MTLDevice>)device
                        library:(id<MTLLibrary>)library
               colorPixelFormat:(MTLPixelFormat)colorPixelFormat
               depthPixelFormat:(MTLPixelFormat)depthPixelFormat
                         bundle:(NSBundle *)bundle NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

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
        simulationSeconds:(float)simulationSeconds;
@end

NS_ASSUME_NONNULL_END
