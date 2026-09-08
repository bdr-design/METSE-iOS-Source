#import <Foundation/Foundation.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

// Native presentation consumer. It receives immutable scalar cue snapshots from
// METSEEngineBridge and never owns weapon, world, visibility, or gameplay state.
@interface METSEAudioPresenter : NSObject
- (BOOL)start;
- (void)stop;
- (void)consumeCueKind:(uint8_t)kind
              material:(uint8_t)material
                  gain:(float)gain
                 pitch:(float)pitch
                   pan:(float)pan
              sequence:(uint64_t)sequence;
@property(nonatomic, readonly) uint64_t droppedVoiceCount;
@end

NS_ASSUME_NONNULL_END
