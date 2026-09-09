#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse { class WorldCollisionCore; class DamageCore; struct DamageResult; }
namespace metse {

struct Projectile {
    bool active=false;
    Vec3 position{};
    Vec3 velocity{};
    double massKg=0.004;
    double ageSeconds=0.0;
    std::uint64_t correlationId=0;
    CombatantId sourceCombatantId=0;
    TeamId sourceTeamId=0;
    FactionId sourceFactionId=0;
    TargetingPolicy targetingPolicy=TargetingPolicy::HostileOnly;
    bool includePlayerTarget=false;
    std::uint8_t penetrations=0;
    std::uint8_t ricochets=0;
};

struct BallisticsMetrics {
    // impacts/worldImpacts are contact counters. A penetrating or ricocheting round
    // can contribute more than one contact. terminalWorldImpacts counts only stops.
    std::uint64_t spawned=0, impacts=0, worldImpacts=0, terminalWorldImpacts=0, targetImpacts=0, penetrations=0, ricochets=0, expired=0, rejectedSpawns=0;
};

struct ProjectileSegmentObservation {
    Vec3 from{};
    Vec3 to{};
    double speedMetersPerSecond=0.0;
    std::uint64_t correlationId=0;
    bool traversed=false;
    bool terminatedAfterSegment=false;
    CombatantId sourceCombatantId=0;
    TeamId sourceTeamId=0;
    FactionId sourceFactionId=0;
};

using ProjectileSegmentObserver = void(*)(void *context,
                                           const ProjectileSegmentObservation& segment) noexcept;

class BallisticsCore final {
public:
    static constexpr std::size_t kMaxProjectiles=128;
    static constexpr std::uint8_t kMaxPenetrationsPerProjectile=2;
    static constexpr std::uint8_t kMaxRicochetsPerProjectile=1;
    static constexpr std::uint8_t kMaxContactsPerStep=4;
    void reset() noexcept;
    bool spawn(const ShotSolution& shot) noexcept;
    void fixedStep(double dt,
                   const WorldCollisionCore& world,
                   DamageCore& damage,
                   ProjectileSegmentObserver observer=nullptr,
                   void *observerContext=nullptr) noexcept;
    [[nodiscard]] const std::array<Projectile,kMaxProjectiles>& projectiles() const noexcept { return projectiles_; }
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] const BallisticsMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] bool validate() const noexcept;
private:
    std::array<Projectile,kMaxProjectiles> projectiles_{};
    BallisticsMetrics metrics_{};
};

} // namespace metse
