#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN
@interface METSEEngineBridge : NSObject <MTKViewDelegate>
- (instancetype)initWithView:(MTKView *)view NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (void)start;
- (void)stop;
- (void)setMoveForward:(float)forward strafe:(float)strafe;
- (void)addLookYaw:(float)yaw pitch:(float)pitch;
- (void)triggerFire;
- (NSString *)statusString;
@end
NS_ASSUME_NONNULL_END
