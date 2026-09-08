#pragma once
#include "METSECharacterMotor.hpp"
#include "METSEWorldCollision.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class AudioCueKind : std::uint8_t {
    Footstep=0,
    FireOutdoor=1,
    FireIndoor=2,
    BulletCrack=3,
    BulletNearMiss=4
};

enum class FXKind : std::uint8_t {
    MuzzleFlash=0,
    SurfaceDust=1
};

struct AudioCue {
    std::uint64_t sequence=0;
    std::uint64_t correlationId=0;
    AudioCueKind kind=AudioCueKind::Footstep;
    Vec3 position{};
    WorldMaterial material=WorldMaterial::Soil;
    double gain=0.0;
    double pitch=1.0;
    bool indoor=false;
};

struct FXInstance {
    bool active=false;
    std::uint64_t sequence=0;
    std::uint64_t correlationId=0;
    FXKind kind=FXKind::MuzzleFlash;
    Vec3 position{};
    Vec3 velocity{};
    WorldMaterial material=WorldMaterial::Soil;
    double ageSeconds=0.0;
    double lifetimeSeconds=0.0;
    double intensity=0.0;
};

struct AudioFXReport {
    std::uint64_t cuesEmitted=0;
    std::uint64_t footsteps=0;
    std::uint64_t outdoorShots=0;
    std::uint64_t indoorShots=0;
    std::uint64_t bulletCracks=0;
    std::uint64_t nearMisses=0;
    std::uint64_t fxSpawned=0;
    std::uint64_t fxReused=0;
    std::uint32_t activeFX=0;
    std::uint32_t retainedCues=0;
};

class AudioFXCore final {
public:
    static constexpr std::size_t kCueCapacity=64;
    static constexpr std::size_t kFXCapacity=48;
    static constexpr std::size_t kProjectileCueMemoryCapacity=64;

    void reset() noexcept;
    void fixedStep(double dt) noexcept;
    void observeMovement(Vec3 previousPosition,Vec3 currentPosition,double horizontalSpeed,CharacterGait gait,const WorldCollisionCore& world) noexcept;
    void observeShot(Vec3 origin,std::uint64_t correlationId,const WorldCollisionCore& world) noexcept;
    void observeProjectileSegment(Vec3 from,Vec3 to,Vec3 listener,double projectileSpeed,std::uint64_t correlationId,bool hostileToListener) noexcept;

    [[nodiscard]] bool cueBySequence(std::uint64_t sequence,AudioCue& out) const noexcept;
    [[nodiscard]] const std::array<FXInstance,kFXCapacity>& fxInstances() const noexcept { return fx_; }
    [[nodiscard]] AudioFXReport report() const noexcept;
    [[nodiscard]] std::uint64_t latestCueSequence() const noexcept { return cueSequence_; }
    [[nodiscard]] std::uint64_t deterministicFingerprint() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    struct ProjectileCueMemory {
        std::uint64_t correlationId=0;
        bool crackEmitted=false;
        bool nearMissEmitted=false;
    };

    void emitCue(AudioCueKind kind,Vec3 position,WorldMaterial material,double gain,double pitch,std::uint64_t correlationId,bool indoor) noexcept;
    void spawnFX(FXKind kind,Vec3 position,Vec3 velocity,WorldMaterial material,double lifetime,double intensity,std::uint64_t correlationId) noexcept;
    ProjectileCueMemory& projectileMemory(std::uint64_t correlationId) noexcept;
    static double stepSpacing(CharacterGait gait) noexcept;
    static double stepGain(CharacterGait gait) noexcept;

    std::array<AudioCue,kCueCapacity> cues_{};
    std::size_t cueWrite_=0;
    std::size_t cueCount_=0;
    std::uint64_t cueSequence_=0;
    std::array<FXInstance,kFXCapacity> fx_{};
    std::uint64_t fxSequence_=0;
    std::array<ProjectileCueMemory,kProjectileCueMemoryCapacity> projectileCueMemory_{};
    std::size_t projectileCueMemoryWrite_=0;
    double stepDistanceAccumulator_=0.0;
    AudioFXReport totals_{};
};

} // namespace metse
