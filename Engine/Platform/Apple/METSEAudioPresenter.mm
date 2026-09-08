#import "METSEAudioPresenter.h"
#import <AVFoundation/AVFoundation.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace {

constexpr std::size_t kAudioVoiceCapacity=12;
constexpr double kSampleRate=48000.0;
constexpr std::uint32_t kReservedVoice=std::numeric_limits<std::uint32_t>::max();
constexpr double kPi=3.14159265358979323846;

struct METSEAudioVoice {
    std::atomic<std::uint32_t> remainingFrames{0};
    std::uint32_t totalFrames=0;
    std::uint32_t renderCursor=0;
    std::uint64_t generation=0;
    std::uint64_t renderedGeneration=0;
    std::uint64_t sequence=0;
    std::uint8_t kind=0;
    std::uint8_t material=0;
    float gain=0.0f;
    float pitch=1.0f;
    float pan=0.0f;
};

struct METSEAudioSharedState {
    std::array<METSEAudioVoice,kAudioVoiceCapacity> voices{};
    std::atomic<std::uint64_t> droppedVoices{0};
    std::atomic<bool> acceptingCues{false};
};

std::uint32_t durationFrames(std::uint8_t kind) noexcept {
    double seconds=0.10;
    switch(kind){
        case 0:seconds=0.11;break; // footstep
        case 1:seconds=0.18;break; // outdoor shot
        case 2:seconds=0.30;break; // indoor shot tail
        case 3:seconds=0.06;break; // supersonic crack
        case 4:seconds=0.14;break; // near-miss pass
        default:break;
    }
    return static_cast<std::uint32_t>(seconds*kSampleRate);
}

float deterministicNoise(std::uint32_t sample,std::uint64_t sequence) noexcept {
    std::uint32_t value=sample*747796405u+2891336453u;
    value^=static_cast<std::uint32_t>(sequence);
    value^=value>>16;
    value*=2246822519u;
    value^=value>>13;
    return static_cast<float>(value&0xffffu)/32767.5f-1.0f;
}

float surfaceFrequency(std::uint8_t material) noexcept {
    constexpr std::array<float,7> frequencies{{142.0f,228.0f,104.0f,128.0f,286.0f,76.0f,116.0f}};
    return frequencies[std::min<std::size_t>(material,frequencies.size()-1)];
}

float synthesize(const METSEAudioVoice& voice,std::uint32_t cursor) noexcept {
    if(voice.totalFrames==0) return 0.0f;
    const float normalized=std::clamp(static_cast<float>(cursor)/
                                      static_cast<float>(voice.totalFrames),0.0f,1.0f);
    const float time=static_cast<float>(cursor/kSampleRate);
    const float decay=1.0f-normalized;
    const float envelope=decay*decay;
    const float noise=deterministicNoise(cursor,voice.sequence);
    const float pitch=std::clamp(voice.pitch,0.5f,1.5f);
    float sample=0.0f;
    switch(voice.kind){
        case 0: {
            const float frequency=surfaceFrequency(voice.material)*pitch;
            const float tonal=std::sin(static_cast<float>(2.0*kPi)*frequency*time);
            const float hardness=voice.material==5?0.22f:(voice.material==1||voice.material==4?0.62f:0.42f);
            sample=(tonal*(1.0f-hardness)+noise*hardness)*envelope;
            break;
        }
        case 1: {
            const float thump=std::sin(static_cast<float>(2.0*kPi)*92.0f*pitch*time);
            sample=(noise*0.74f+thump*0.46f)*envelope;
            break;
        }
        case 2: {
            const float thump=std::sin(static_cast<float>(2.0*kPi)*78.0f*pitch*time);
            const float reflection=std::sin(static_cast<float>(2.0*kPi)*173.0f*time)*(0.35f+0.65f*decay);
            sample=(noise*0.54f+thump*0.40f+reflection*0.22f)*envelope;
            break;
        }
        case 3: {
            const float impulse=cursor<12?1.0f:0.0f;
            sample=(impulse+noise*0.70f)*envelope;
            break;
        }
        case 4: {
            const float frequency=(1450.0f-1050.0f*normalized)*pitch;
            const float pass=std::sin(static_cast<float>(2.0*kPi)*frequency*time);
            const float shaped=1.0f-std::abs(normalized*2.0f-1.0f);
            sample=(pass*0.58f+noise*0.22f)*shaped*decay;
            break;
        }
        default:break;
    }
    return std::clamp(sample*voice.gain,-1.0f,1.0f);
}

void addStereoSample(AudioBufferList *outputData,
                     AVAudioFrameCount frame,
                     float left,
                     float right) noexcept {
    if(outputData==nullptr||outputData->mNumberBuffers==0) return;
    if(outputData->mNumberBuffers==1){
        auto& buffer=outputData->mBuffers[0];
        auto *samples=static_cast<float *>(buffer.mData);
        if(samples==nullptr) return;
        const std::uint32_t channels=std::max<std::uint32_t>(1,buffer.mNumberChannels);
        if(channels==1){
            samples[frame]+=(left+right)*0.5f;
        }else{
            samples[static_cast<std::size_t>(frame)*channels]+=left;
            samples[static_cast<std::size_t>(frame)*channels+1]+=right;
        }
        return;
    }
    auto *leftSamples=static_cast<float *>(outputData->mBuffers[0].mData);
    auto *rightSamples=static_cast<float *>(outputData->mBuffers[1].mData);
    if(leftSamples!=nullptr) leftSamples[frame]+=left;
    if(rightSamples!=nullptr) rightSamples[frame]+=right;
}

void clampOutput(AudioBufferList *outputData) noexcept {
    if(outputData==nullptr) return;
    for(std::uint32_t bufferIndex=0;bufferIndex<outputData->mNumberBuffers;++bufferIndex){
        auto& buffer=outputData->mBuffers[bufferIndex];
        auto *samples=static_cast<float *>(buffer.mData);
        if(samples==nullptr) continue;
        const std::size_t sampleCount=buffer.mDataByteSize/sizeof(float);
        for(std::size_t sample=0;sample<sampleCount;++sample){
            samples[sample]=std::clamp(samples[sample],-1.0f,1.0f);
        }
    }
}

OSStatus renderAudio(METSEAudioSharedState *state,
                     BOOL *isSilence,
                     AVAudioFrameCount frameCount,
                     AudioBufferList *outputData) noexcept {
    if(outputData==nullptr) return noErr;
    for(std::uint32_t bufferIndex=0;bufferIndex<outputData->mNumberBuffers;++bufferIndex){
        auto& buffer=outputData->mBuffers[bufferIndex];
        if(buffer.mData!=nullptr) std::memset(buffer.mData,0,buffer.mDataByteSize);
    }

    bool active=false;
    if(state!=nullptr){
        for(auto& voice:state->voices){
            const std::uint32_t remaining=voice.remainingFrames.load(std::memory_order_acquire);
            if(remaining==0||remaining==kReservedVoice) continue;
            active=true;
            if(voice.renderedGeneration!=voice.generation){
                voice.renderedGeneration=voice.generation;
                voice.renderCursor=0;
            }
            const std::uint32_t framesToRender=std::min<std::uint32_t>(remaining,frameCount);
            const float pan=std::clamp(voice.pan,-1.0f,1.0f);
            const float leftGain=std::sqrt((1.0f-pan)*0.5f);
            const float rightGain=std::sqrt((1.0f+pan)*0.5f);
            for(std::uint32_t frame=0;frame<framesToRender;++frame){
                const float sample=synthesize(voice,voice.renderCursor++);
                addStereoSample(outputData,frame,sample*leftGain,sample*rightGain);
            }
            voice.remainingFrames.store(remaining-framesToRender,std::memory_order_release);
        }
    }
    clampOutput(outputData);
    if(isSilence!=nullptr) *isSilence=active?NO:YES;
    return noErr;
}

void clearVoices(METSEAudioSharedState& state) noexcept {
    for(auto& voice:state.voices){
        voice.remainingFrames.store(0,std::memory_order_release);
    }
}

} // namespace

@implementation METSEAudioPresenter {
    AVAudioEngine *_engine;
    AVAudioSourceNode *_sourceNode;
    AVAudioFormat *_format;
    std::shared_ptr<METSEAudioSharedState> _state;
}

- (instancetype)init {
    self=[super init];
    if(!self) return nil;
    _state=std::make_shared<METSEAudioSharedState>();
    _engine=[AVAudioEngine new];
    _format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:kSampleRate channels:2];
    if(!_engine||!_format) return nil;
    const std::shared_ptr<METSEAudioSharedState> sharedState=_state;
    _sourceNode=[[AVAudioSourceNode alloc] initWithFormat:_format
                                            renderBlock:^OSStatus(BOOL *isSilence,
                                                                  const AudioTimeStamp *timestamp,
                                                                  AVAudioFrameCount frameCount,
                                                                  AudioBufferList *outputData) {
        (void)timestamp;
        return renderAudio(sharedState.get(),isSilence,frameCount,outputData);
    }];
    if(!_sourceNode) return nil;
    [_engine attachNode:_sourceNode];
    [_engine connect:_sourceNode to:_engine.mainMixerNode format:_format];
    _engine.mainMixerNode.outputVolume=0.72f;
    return self;
}

- (BOOL)start {
    if(_engine.isRunning){
        _state->acceptingCues.store(true,std::memory_order_release);
        return YES;
    }
    AVAudioSession *session=AVAudioSession.sharedInstance;
    NSError *sessionError=nil;
    [session setCategory:AVAudioSessionCategoryAmbient
                    mode:AVAudioSessionModeDefault
                 options:AVAudioSessionCategoryOptionMixWithOthers
                   error:&sessionError];
    if(sessionError) NSLog(@"METSE audio session configuration failed: %@",sessionError);
    sessionError=nil;
    [session setActive:YES error:&sessionError];
    if(sessionError) NSLog(@"METSE audio session activation failed: %@",sessionError);

    NSError *engineError=nil;
    const BOOL started=[_engine startAndReturnError:&engineError];
    _state->acceptingCues.store(started,std::memory_order_release);
    if(!started) NSLog(@"METSE native audio engine failed: %@",engineError);
    return started;
}

- (void)stop {
    _state->acceptingCues.store(false,std::memory_order_release);
    [_engine stop];
    clearVoices(*_state);
    NSError *error=nil;
    [AVAudioSession.sharedInstance setActive:NO
                                  withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                        error:&error];
    if(error) NSLog(@"METSE audio session deactivation failed: %@",error);
}

- (void)consumeCueKind:(uint8_t)kind
              material:(uint8_t)material
                  gain:(float)gain
                 pitch:(float)pitch
                   pan:(float)pan
              sequence:(uint64_t)sequence {
    if(kind>4||material>6||sequence==0||!std::isfinite(gain)||!std::isfinite(pitch)||
       !std::isfinite(pan)||!_state->acceptingCues.load(std::memory_order_acquire)){
        _state->droppedVoices.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    for(auto& voice:_state->voices){
        std::uint32_t expected=0;
        if(!voice.remainingFrames.compare_exchange_strong(expected,kReservedVoice,
                                                           std::memory_order_acq_rel)) continue;
        voice.totalFrames=durationFrames(kind);
        voice.sequence=sequence;
        voice.kind=kind;
        voice.material=material;
        voice.gain=std::clamp(gain,0.0f,1.0f);
        voice.pitch=std::clamp(pitch,0.5f,1.5f);
        voice.pan=std::clamp(pan,-1.0f,1.0f);
        ++voice.generation;
        voice.remainingFrames.store(voice.totalFrames,std::memory_order_release);
        return;
    }
    // Fixed voice pool: when all voices are busy, the newest presentation cue drops.
    _state->droppedVoices.fetch_add(1,std::memory_order_relaxed);
}

- (uint64_t)droppedVoiceCount {
    return _state->droppedVoices.load(std::memory_order_relaxed);
}

- (void)dealloc {
    [self stop];
}

@end
