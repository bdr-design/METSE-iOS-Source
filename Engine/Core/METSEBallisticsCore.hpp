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
    std::uint8_t penetrations=0;
};

struct BallisticsMetrics {
    std::uint64_t spawned=0, impacts=0, worldImpacts=0, targetImpacts=0, penetrations=0, expired=0, rejectedSpawns=0;
};

class BallisticsCore final {
public:
    static constexpr std::size_t kMaxProjectiles=128;
    void reset() noexcept;
    bool spawn(const ShotSolution& shot) noexcept;
    void fixedStep(double dt,const WorldCollisionCore& world,DamageCore& damage) noexcept;
    [[nodiscard]] const std::array<Projectile,kMaxProjectiles>& projectiles() const noexcept { return projectiles_; }
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] const BallisticsMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] bool validate() const noexcept;
private:
    std::array<Projectile,kMaxProjectiles> projectiles_{};
    BallisticsMetrics metrics_{};
};

} // namespace metse
