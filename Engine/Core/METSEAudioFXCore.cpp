#include "METSEAudioFXCore.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace metse {
namespace {

bool finiteVec(Vec3 value) noexcept {
    return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
}

double horizontalDistance(Vec3 a,Vec3 b) noexcept {
    return std::hypot(b.x-a.x,b.z-a.z);
}

double pointSegmentDistance(Vec3 point,Vec3 a,Vec3 b,Vec3& closest) noexcept {
    const Vec3 ab{b.x-a.x,b.y-a.y,b.z-a.z};
    const Vec3 ap{point.x-a.x,point.y-a.y,point.z-a.z};
    const double lengthSquared=ab.x*ab.x+ab.y*ab.y+ab.z*ab.z;
    double t=0.0;
    if(lengthSquared>1e-12){
        t=std::clamp((ap.x*ab.x+ap.y*ab.y+ap.z*ab.z)/lengthSquared,0.0,1.0);
    }
    closest={a.x+ab.x*t,a.y+ab.y*t,a.z+ab.z*t};
    const double dx=point.x-closest.x,dy=point.y-closest.y,dz=point.z-closest.z;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}

void mix64(std::uint64_t& hash,std::uint64_t value) noexcept {
    constexpr std::uint64_t kPrime=1099511628211ull;
    for(int i=0;i<8;++i){
        hash^=static_cast<std::uint8_t>(value>>(i*8));
        hash*=kPrime;
    }
}

void mixDouble(std::uint64_t& hash,double value) noexcept {
    mix64(hash,std::bit_cast<std::uint64_t>(value));
}

} // namespace

void AudioFXCore::reset() noexcept {
    cues_={};
    cueWrite_=0;
    cueCount_=0;
    cueSequence_=0;
    fx_={};
    fxSequence_=0;
    projectileCueMemory_={};
    projectileCueMemoryWrite_=0;
    stepDistanceAccumulator_=0.0;
    totals_={};
}

double AudioFXCore::footstepSpacingMeters(CharacterGait gait) noexcept {
    switch(gait){
        case CharacterGait::Walk:return 1.15;
        case CharacterGait::Tactical:return 1.00;
        case CharacterGait::Jog:return 0.82;
        case CharacterGait::Sprint:return 0.72;
        case CharacterGait::Crouch:return 1.20;
        case CharacterGait::Idle:
        case CharacterGait::Crawl:return 0.0;
    }
    return 0.0;
}

double AudioFXCore::footstepGain(CharacterGait gait) noexcept {
    switch(gait){
        case CharacterGait::Walk:return 0.30;
        case CharacterGait::Tactical:return 0.38;
        case CharacterGait::Jog:return 0.58;
        case CharacterGait::Sprint:return 0.82;
        case CharacterGait::Crouch:return 0.22;
        case CharacterGait::Idle:
        case CharacterGait::Crawl:return 0.0;
    }
    return 0.0;
}

void AudioFXCore::emitCue(AudioCueKind kind,
                          Vec3 position,
                          WorldMaterial material,
                          double gain,
                          double pitch,
                          std::uint64_t correlationId,
                          bool indoor) noexcept {
    ++cueSequence_;
    AudioCue cue{};
    cue.sequence=cueSequence_;
    cue.correlationId=correlationId;
    cue.kind=kind;
    cue.position=position;
    cue.material=material;
    cue.gain=std::clamp(gain,0.0,1.0);
    cue.pitch=std::clamp(pitch,0.5,1.5);
    cue.indoor=indoor;
    cues_[cueWrite_]=cue;
    cueWrite_=(cueWrite_+1)%kCueCapacity;
    cueCount_=std::min(cueCount_+1,kCueCapacity);
    ++totals_.cuesEmitted;
    switch(kind){
        case AudioCueKind::Footstep:++totals_.footsteps;break;
        case AudioCueKind::FireOutdoor:++totals_.outdoorShots;break;
        case AudioCueKind::FireIndoor:++totals_.indoorShots;break;
        case AudioCueKind::BulletCrack:++totals_.bulletCracks;break;
        case AudioCueKind::BulletNearMiss:++totals_.nearMisses;break;
    }
}

bool AudioFXCore::spawnFX(FXKind kind,
                          Vec3 position,
                          Vec3 velocity,
                          WorldMaterial material,
                          double lifetime,
                          double intensity,
                          std::uint64_t correlationId) noexcept {
    ++totals_.fxSpawnRequests;
    if(!finiteVec(position)||!finiteVec(velocity)||!std::isfinite(lifetime)||!std::isfinite(intensity)||
       lifetime<=0.0||intensity<=0.0){
        ++totals_.fxDropped;
        return false;
    }

    std::size_t slot=kFXCapacity;
    for(std::size_t i=0;i<kFXCapacity;++i){
        if(!fx_[i].active){slot=i;break;}
    }
    // Deterministic drop-new policy: presentation pressure can never block or evict
    // an already-published effect midway through its bounded lifetime.
    if(slot==kFXCapacity){
        ++totals_.fxDropped;
        return false;
    }

    ++fxSequence_;
    fx_[slot]={true,fxSequence_,correlationId,kind,position,velocity,material,0.0,lifetime,
               std::clamp(intensity,0.0,1.0)};
    ++totals_.fxSpawned;
    return true;
}

void AudioFXCore::fixedStep(double dt) noexcept {
    if(!std::isfinite(dt)||dt<=0.0) return;
    for(auto& effect:fx_){
        if(!effect.active) continue;
        effect.ageSeconds+=dt;
        effect.position.x+=effect.velocity.x*dt;
        effect.position.y+=effect.velocity.y*dt;
        effect.position.z+=effect.velocity.z*dt;
        if(effect.ageSeconds>=effect.lifetimeSeconds) effect.active=false;
    }
}

void AudioFXCore::observeMovement(Vec3 previousPosition,
                                  Vec3 currentPosition,
                                  double horizontalSpeed,
                                  CharacterGait gait,
                                  bool grounded,
                                  const WorldCollisionCore& world) noexcept {
    if(!finiteVec(previousPosition)||!finiteVec(currentPosition)||!std::isfinite(horizontalSpeed)) return;
    const double spacing=footstepSpacingMeters(gait);
    if(!grounded||spacing<=0.0||horizontalSpeed<0.15){
        stepDistanceAccumulator_=0.0;
        return;
    }

    const double traveled=horizontalDistance(previousPosition,currentPosition);
    // A one-slice displacement this large is a teleport/reset, not a physical footstep.
    if(!std::isfinite(traveled)||traveled>2.0){
        stepDistanceAccumulator_=0.0;
        return;
    }
    stepDistanceAccumulator_+=traveled;
    if(stepDistanceAccumulator_+1e-9<spacing) return;
    stepDistanceAccumulator_=std::max(0.0,stepDistanceAccumulator_-spacing);
    if(stepDistanceAccumulator_>=spacing){
        stepDistanceAccumulator_=std::fmod(stepDistanceAccumulator_,spacing);
    }

    const WorldMaterial material=world.surfaceMaterialAt(currentPosition.x,currentPosition.z);
    const double gain=footstepGain(gait);
    const double pitch=0.97+static_cast<double>((cueSequence_+1)%5)*0.015;
    emitCue(AudioCueKind::Footstep,currentPosition,material,gain,pitch,0,false);
    if((material==WorldMaterial::Soil||material==WorldMaterial::Rock)&&gain>=0.25){
        (void)spawnFX(FXKind::SurfaceDust,{currentPosition.x,0.08,currentPosition.z},
                      {0.0,0.35,0.0},material,0.30,gain,0);
    }
}

void AudioFXCore::observeShot(Vec3 origin,
                              std::uint64_t correlationId,
                              const WorldCollisionCore& world) noexcept {
    if(!finiteVec(origin)||correlationId==0) return;
    const auto acoustic=world.acousticProbeAt(origin);
    const auto kind=acoustic.indoor?AudioCueKind::FireIndoor:AudioCueKind::FireOutdoor;
    emitCue(kind,origin,world.surfaceMaterialAt(origin.x,origin.z),1.0,
            acoustic.indoor?0.94:1.0,correlationId,acoustic.indoor);
    (void)spawnFX(FXKind::MuzzleFlash,origin,{0.0,0.0,0.0},WorldMaterial::Steel,
                  0.055,1.0,correlationId);
}

AudioFXCore::ProjectileCueMemory& AudioFXCore::projectileMemory(std::uint64_t correlationId) noexcept {
    for(auto& memory:projectileCueMemory_){
        if(memory.correlationId==correlationId) return memory;
    }
    auto& slot=projectileCueMemory_[projectileCueMemoryWrite_];
    projectileCueMemoryWrite_=(projectileCueMemoryWrite_+1)%kProjectileCueMemoryCapacity;
    slot={correlationId,false,false,false};
    return slot;
}

void AudioFXCore::observeProjectileSegment(const ProjectileSegmentObservation& segment,
                                           Vec3 listener,
                                           bool hostileToListener) noexcept {
    if(!hostileToListener||segment.correlationId==0||!finiteVec(segment.from)||!finiteVec(segment.to)||
       !finiteVec(listener)||!std::isfinite(segment.speedMetersPerSecond)||
       segment.speedMetersPerSecond<0.0) return;

    auto& memory=projectileMemory(segment.correlationId);
    if(memory.terminated) return;
    if(!segment.traversed){
        if(segment.terminatedAfterSegment) memory.terminated=true;
        return;
    }

    Vec3 closest{};
    const double distance=pointSegmentDistance(listener,segment.from,segment.to,closest);
    if(!std::isfinite(distance)) return;
    if(segment.speedMetersPerSecond>=kCrackMinimumSpeedMetersPerSecond&&
       distance<=kCrackRadiusMeters&&!memory.crackEmitted){
        memory.crackEmitted=true;
        const double gain=std::clamp(1.0-distance/(kCrackRadiusMeters+2.0),0.18,1.0);
        emitCue(AudioCueKind::BulletCrack,closest,WorldMaterial::Soil,gain,1.0,
                segment.correlationId,false);
    }
    if(segment.speedMetersPerSecond>=kNearMissMinimumSpeedMetersPerSecond&&
       distance<=kNearMissRadiusMeters&&!memory.nearMissEmitted){
        memory.nearMissEmitted=true;
        const double gain=std::clamp(1.0-distance/(kNearMissRadiusMeters+0.5),0.25,1.0);
        emitCue(AudioCueKind::BulletNearMiss,closest,WorldMaterial::Soil,gain,0.98,
                segment.correlationId,false);
    }
    if(segment.terminatedAfterSegment) memory.terminated=true;
}

bool AudioFXCore::cueBySequence(std::uint64_t sequence,AudioCue& out) const noexcept {
    if(sequence==0||sequence>cueSequence_||cueCount_==0) return false;
    const std::uint64_t oldest=cueSequence_-static_cast<std::uint64_t>(cueCount_)+1;
    if(sequence<oldest) return false;
    const std::uint64_t offset=cueSequence_-sequence;
    const std::size_t index=(cueWrite_+kCueCapacity-1-static_cast<std::size_t>(offset))%kCueCapacity;
    if(cues_[index].sequence!=sequence) return false;
    out=cues_[index];
    return true;
}

AudioFXReport AudioFXCore::report() const noexcept {
    AudioFXReport out=totals_;
    out.retainedCues=static_cast<std::uint32_t>(cueCount_);
    for(const auto& effect:fx_){
        if(effect.active) ++out.activeFX;
    }
    return out;
}

std::uint64_t AudioFXCore::deterministicFingerprint() const noexcept {
    std::uint64_t hash=1469598103934665603ull;
    mix64(hash,cueSequence_);
    mix64(hash,fxSequence_);
    mix64(hash,static_cast<std::uint64_t>(cueWrite_));
    mix64(hash,static_cast<std::uint64_t>(cueCount_));
    mix64(hash,static_cast<std::uint64_t>(projectileCueMemoryWrite_));
    mixDouble(hash,stepDistanceAccumulator_);
    mix64(hash,totals_.cuesEmitted);
    mix64(hash,totals_.footsteps);
    mix64(hash,totals_.outdoorShots);
    mix64(hash,totals_.indoorShots);
    mix64(hash,totals_.bulletCracks);
    mix64(hash,totals_.nearMisses);
    mix64(hash,totals_.fxSpawnRequests);
    mix64(hash,totals_.fxSpawned);
    mix64(hash,totals_.fxDropped);

    const std::uint64_t oldest=cueCount_?cueSequence_-static_cast<std::uint64_t>(cueCount_)+1:0;
    for(std::size_t i=0;i<cueCount_;++i){
        AudioCue cue{};
        if(!cueBySequence(oldest+static_cast<std::uint64_t>(i),cue)) continue;
        mix64(hash,cue.sequence);
        mix64(hash,cue.correlationId);
        mix64(hash,static_cast<std::uint64_t>(cue.kind));
        mix64(hash,static_cast<std::uint64_t>(cue.material));
        mixDouble(hash,cue.position.x);
        mixDouble(hash,cue.position.y);
        mixDouble(hash,cue.position.z);
        mixDouble(hash,cue.gain);
        mixDouble(hash,cue.pitch);
        mix64(hash,cue.indoor?1u:0u);
    }
    for(const auto& effect:fx_){
        if(!effect.active) continue;
        mix64(hash,effect.sequence);
        mix64(hash,effect.correlationId);
        mix64(hash,static_cast<std::uint64_t>(effect.kind));
        mix64(hash,static_cast<std::uint64_t>(effect.material));
        mixDouble(hash,effect.position.x);
        mixDouble(hash,effect.position.y);
        mixDouble(hash,effect.position.z);
        mixDouble(hash,effect.velocity.x);
        mixDouble(hash,effect.velocity.y);
        mixDouble(hash,effect.velocity.z);
        mixDouble(hash,effect.ageSeconds);
        mixDouble(hash,effect.lifetimeSeconds);
        mixDouble(hash,effect.intensity);
    }
    for(const auto& memory:projectileCueMemory_){
        mix64(hash,memory.correlationId);
        mix64(hash,memory.crackEmitted?1u:0u);
        mix64(hash,memory.nearMissEmitted?1u:0u);
        mix64(hash,memory.terminated?1u:0u);
    }
    return hash;
}

bool AudioFXCore::validate() const noexcept {
    if(cueWrite_>=kCueCapacity||cueCount_>kCueCapacity||
       projectileCueMemoryWrite_>=kProjectileCueMemoryCapacity||
       !std::isfinite(stepDistanceAccumulator_)||stepDistanceAccumulator_<0.0||
       stepDistanceAccumulator_>1.20+1e-8||totals_.cuesEmitted!=cueSequence_||
       totals_.fxSpawned!=fxSequence_||
       totals_.fxSpawnRequests!=totals_.fxSpawned+totals_.fxDropped||
       totals_.footsteps+totals_.outdoorShots+totals_.indoorShots+
           totals_.bulletCracks+totals_.nearMisses!=totals_.cuesEmitted) return false;

    if(cueCount_>0){
        const std::uint64_t oldest=cueSequence_-static_cast<std::uint64_t>(cueCount_)+1;
        for(std::size_t i=0;i<cueCount_;++i){
            AudioCue cue{};
            if(!cueBySequence(oldest+static_cast<std::uint64_t>(i),cue)||
               !finiteVec(cue.position)||!std::isfinite(cue.gain)||!std::isfinite(cue.pitch)||
               cue.gain<0.0||cue.gain>1.0||cue.pitch<0.5||cue.pitch>1.5||
               static_cast<std::uint8_t>(cue.kind)>static_cast<std::uint8_t>(AudioCueKind::BulletNearMiss)||
               static_cast<std::uint8_t>(cue.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
            if(cue.kind!=AudioCueKind::Footstep&&cue.correlationId==0) return false;
            if(cue.indoor!=(cue.kind==AudioCueKind::FireIndoor)) return false;
        }
    }

    std::uint32_t activeEffects=0;
    for(const auto& effect:fx_){
        if(!effect.active) continue;
        ++activeEffects;
        if(effect.sequence==0||!finiteVec(effect.position)||!finiteVec(effect.velocity)||
           !std::isfinite(effect.ageSeconds)||!std::isfinite(effect.lifetimeSeconds)||
           !std::isfinite(effect.intensity)||effect.ageSeconds<0.0||effect.lifetimeSeconds<=0.0||
           effect.ageSeconds>=effect.lifetimeSeconds+1e-9||effect.intensity<=0.0||effect.intensity>1.0||
           static_cast<std::uint8_t>(effect.kind)>static_cast<std::uint8_t>(FXKind::SurfaceDust)||
           static_cast<std::uint8_t>(effect.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
        if(effect.kind==FXKind::MuzzleFlash&&effect.correlationId==0) return false;
    }
    if(activeEffects>kFXCapacity) return false;

    for(const auto& memory:projectileCueMemory_){
        if(memory.correlationId==0&&(memory.crackEmitted||memory.nearMissEmitted||memory.terminated)) return false;
    }
    return true;
}

} // namespace metse
