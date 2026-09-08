#include "METSEAudioFXCore.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace metse {
namespace {

bool finiteVec(Vec3 v) noexcept {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}

double horizontalDistance(Vec3 a,Vec3 b) noexcept {
    return std::hypot(b.x-a.x,b.z-a.z);
}

double pointSegmentDistance(Vec3 point,Vec3 a,Vec3 b,Vec3& closest) noexcept {
    const Vec3 ab{b.x-a.x,b.y-a.y,b.z-a.z};
    const Vec3 ap{point.x-a.x,point.y-a.y,point.z-a.z};
    const double lengthSquared=ab.x*ab.x+ab.y*ab.y+ab.z*ab.z;
    double t=0.0;
    if(lengthSquared>1e-12) t=std::clamp((ap.x*ab.x+ap.y*ab.y+ap.z*ab.z)/lengthSquared,0.0,1.0);
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

double AudioFXCore::stepSpacing(CharacterGait gait) noexcept {
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

double AudioFXCore::stepGain(CharacterGait gait) noexcept {
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

void AudioFXCore::emitCue(AudioCueKind kind,Vec3 position,WorldMaterial material,double gain,double pitch,std::uint64_t correlationId,bool indoor) noexcept {
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

void AudioFXCore::spawnFX(FXKind kind,Vec3 position,Vec3 velocity,WorldMaterial material,double lifetime,double intensity,std::uint64_t correlationId) noexcept {
    if(!finiteVec(position)||!finiteVec(velocity)||!std::isfinite(lifetime)||!std::isfinite(intensity)||lifetime<=0.0||intensity<=0.0) return;
    std::size_t slot=kFXCapacity;
    for(std::size_t i=0;i<kFXCapacity;++i){
        if(!fx_[i].active){slot=i;break;}
    }
    if(slot==kFXCapacity){
        slot=0;
        for(std::size_t i=1;i<kFXCapacity;++i){
            if(fx_[i].ageSeconds>fx_[slot].ageSeconds) slot=i;
        }
        ++totals_.fxReused;
    }
    ++fxSequence_;
    fx_[slot]={true,fxSequence_,correlationId,kind,position,velocity,material,0.0,lifetime,std::clamp(intensity,0.0,1.0)};
    ++totals_.fxSpawned;
}

void AudioFXCore::fixedStep(double dt) noexcept {
    if(!std::isfinite(dt)||dt<=0.0) return;
    for(auto& fx:fx_){
        if(!fx.active) continue;
        fx.ageSeconds+=dt;
        fx.position.x+=fx.velocity.x*dt;
        fx.position.y+=fx.velocity.y*dt;
        fx.position.z+=fx.velocity.z*dt;
        if(fx.ageSeconds>=fx.lifetimeSeconds) fx.active=false;
    }
}

void AudioFXCore::observeMovement(Vec3 previousPosition,Vec3 currentPosition,double horizontalSpeed,CharacterGait gait,const WorldCollisionCore& world) noexcept {
    if(!finiteVec(previousPosition)||!finiteVec(currentPosition)||!std::isfinite(horizontalSpeed)) return;
    const double spacing=stepSpacing(gait);
    if(spacing<=0.0||horizontalSpeed<0.15){
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
    stepDistanceAccumulator_=std::fmod(stepDistanceAccumulator_,spacing);
    const WorldMaterial material=world.surfaceMaterialAt(currentPosition.x,currentPosition.z);
    const double gain=stepGain(gait);
    const double pitch=0.97+static_cast<double>((cueSequence_+1)%5)*0.015;
    emitCue(AudioCueKind::Footstep,currentPosition,material,gain,pitch,0,false);
    if((material==WorldMaterial::Soil||material==WorldMaterial::Rock)&&gain>=0.25){
        spawnFX(FXKind::SurfaceDust,{currentPosition.x,0.08,currentPosition.z},{0.0,0.35,0.0},material,0.30,gain,0);
    }
}

void AudioFXCore::observeShot(Vec3 origin,std::uint64_t correlationId,const WorldCollisionCore& world) noexcept {
    if(!finiteVec(origin)||correlationId==0) return;
    const bool indoor=world.hasOverheadCover(origin,5.0);
    const auto kind=indoor?AudioCueKind::FireIndoor:AudioCueKind::FireOutdoor;
    emitCue(kind,origin,world.surfaceMaterialAt(origin.x,origin.z),1.0,indoor?0.94:1.0,correlationId,indoor);
    spawnFX(FXKind::MuzzleFlash,origin,{0.0,0.0,0.0},WorldMaterial::Steel,0.055,1.0,correlationId);
}

AudioFXCore::ProjectileCueMemory& AudioFXCore::projectileMemory(std::uint64_t correlationId) noexcept {
    for(auto& memory:projectileCueMemory_){
        if(memory.correlationId==correlationId) return memory;
    }
    auto& slot=projectileCueMemory_[projectileCueMemoryWrite_];
    projectileCueMemoryWrite_=(projectileCueMemoryWrite_+1)%kProjectileCueMemoryCapacity;
    slot={correlationId,false,false};
    return slot;
}

void AudioFXCore::observeProjectileSegment(Vec3 from,Vec3 to,Vec3 listener,double projectileSpeed,std::uint64_t correlationId,bool hostileToListener) noexcept {
    if(!hostileToListener||correlationId==0||!finiteVec(from)||!finiteVec(to)||!finiteVec(listener)||
       !std::isfinite(projectileSpeed)||projectileSpeed<=0.0) return;
    Vec3 closest{};
    const double distance=pointSegmentDistance(listener,from,to,closest);
    if(!std::isfinite(distance)) return;
    auto& memory=projectileMemory(correlationId);
    if(projectileSpeed>=360.0&&distance<=12.0&&!memory.crackEmitted){
        memory.crackEmitted=true;
        const double gain=std::clamp(1.0-distance/14.0,0.18,1.0);
        emitCue(AudioCueKind::BulletCrack,closest,WorldMaterial::Soil,gain,1.0,correlationId,false);
    }
    if(projectileSpeed>=80.0&&distance<=2.25&&!memory.nearMissEmitted){
        memory.nearMissEmitted=true;
        const double gain=std::clamp(1.0-distance/2.75,0.25,1.0);
        emitCue(AudioCueKind::BulletNearMiss,closest,WorldMaterial::Soil,gain,0.98,correlationId,false);
    }
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
    for(const auto& fx:fx_) if(fx.active) ++out.activeFX;
    return out;
}

std::uint64_t AudioFXCore::deterministicFingerprint() const noexcept {
    std::uint64_t hash=1469598103934665603ull;
    mix64(hash,cueSequence_);
    mix64(hash,fxSequence_);
    mixDouble(hash,stepDistanceAccumulator_);
    mix64(hash,static_cast<std::uint64_t>(cueCount_));
    const std::uint64_t oldest=cueCount_?cueSequence_-static_cast<std::uint64_t>(cueCount_)+1:0;
    for(std::uint64_t sequence=oldest;sequence!=0&&sequence<=cueSequence_;++sequence){
        AudioCue cue{};
        if(!cueBySequence(sequence,cue)) continue;
        mix64(hash,cue.sequence);
        mix64(hash,cue.correlationId);
        mix64(hash,static_cast<std::uint64_t>(cue.kind));
        mix64(hash,static_cast<std::uint64_t>(cue.material));
        mixDouble(hash,cue.position.x);mixDouble(hash,cue.position.y);mixDouble(hash,cue.position.z);
        mixDouble(hash,cue.gain);mixDouble(hash,cue.pitch);mix64(hash,cue.indoor?1u:0u);
    }
    for(const auto& fx:fx_){
        if(!fx.active) continue;
        mix64(hash,fx.sequence);mix64(hash,fx.correlationId);mix64(hash,static_cast<std::uint64_t>(fx.kind));
        mixDouble(hash,fx.position.x);mixDouble(hash,fx.position.y);mixDouble(hash,fx.position.z);
        mixDouble(hash,fx.ageSeconds);mixDouble(hash,fx.lifetimeSeconds);mixDouble(hash,fx.intensity);
    }
    return hash;
}

bool AudioFXCore::validate() const noexcept {
    if(cueWrite_>=kCueCapacity||cueCount_>kCueCapacity||projectileCueMemoryWrite_>=kProjectileCueMemoryCapacity||
       !std::isfinite(stepDistanceAccumulator_)||stepDistanceAccumulator_<0.0||totals_.cuesEmitted!=cueSequence_||totals_.fxSpawned!=fxSequence_) return false;
    if(cueCount_>0){
        const std::uint64_t oldest=cueSequence_-static_cast<std::uint64_t>(cueCount_)+1;
        for(std::uint64_t sequence=oldest;sequence<=cueSequence_;++sequence){
            AudioCue cue{};
            if(!cueBySequence(sequence,cue)||!finiteVec(cue.position)||!std::isfinite(cue.gain)||!std::isfinite(cue.pitch)||
               cue.gain<0.0||cue.gain>1.0||cue.pitch<0.5||cue.pitch>1.5||
               static_cast<std::uint8_t>(cue.kind)>static_cast<std::uint8_t>(AudioCueKind::BulletNearMiss)||
               static_cast<std::uint8_t>(cue.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
        }
    }
    for(const auto& fx:fx_){
        if(!fx.active) continue;
        if(fx.sequence==0||!finiteVec(fx.position)||!finiteVec(fx.velocity)||!std::isfinite(fx.ageSeconds)||
           !std::isfinite(fx.lifetimeSeconds)||!std::isfinite(fx.intensity)||fx.ageSeconds<0.0||fx.lifetimeSeconds<=0.0||
           fx.ageSeconds>=fx.lifetimeSeconds+1e-9||fx.intensity<=0.0||fx.intensity>1.0||
           static_cast<std::uint8_t>(fx.kind)>static_cast<std::uint8_t>(FXKind::SurfaceDust)||
           static_cast<std::uint8_t>(fx.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
    }
    return true;
}

} // namespace metse
