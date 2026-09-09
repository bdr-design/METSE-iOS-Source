#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
NS_ASSUME_NONNULL_BEGIN
@interface METSEEngineBridge : NSObject <MTKViewDelegate>
- (nullable instancetype)initWithView:(MTKView *)view NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (void)start;
- (void)stop;
- (void)setMoveForward:(float)forward strafe:(float)strafe;
- (void)addLookYaw:(float)yaw pitch:(float)pitch;
- (void)setSprintHeld:(BOOL)held;
- (void)setAimHeld:(BOOL)held;
- (void)cycleStance;
- (void)triggerFire;
- (void)reloadWeapon;
- (NSString *)statusString;
- (NSString *)stanceName;
- (NSDictionary<NSString *, id> *)observatorySnapshot;
- (NSString *)observatoryReportText;
@end
NS_ASSUME_NONNULL_END
