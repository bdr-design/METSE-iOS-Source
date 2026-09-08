#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class HitRegion : std::uint8_t { None=0, Head, Neck, Thorax, Abdomen, Arm, Leg };
enum class ArmorZone : std::uint8_t { None=0, Helmet, Torso };
enum class CombatState : std::uint8_t { Effective=0, Wounded, Incapacitated, Dead };
enum class DamageCause : std::uint8_t { Impact=0, Bleeding };

struct DamageTarget {
    std::uint32_t id = 0;
    Vec3 position{};
    double health = 100.0;
    double radius = 0.34;
    double helmetArmorJoules = 420.0;
    double torsoArmorJoules = 900.0;
    double bleedingPerSecond = 0.0;
    std::uint64_t lastDamageCorrelationId = 0;
    CombatState combatState = CombatState::Effective;
    bool alive = true;
};

struct DamageIntersection {
    bool hit = false;
    double t = 2.0;
    std::size_t targetIndex = 0;
    std::uint32_t targetId = 0;
    HitRegion region = HitRegion::None;
    Vec3 point{};
    double radialDistance = 0.0;
};

struct DamageResult {
    std::uint64_t sequence = 0;
    bool hit = false;
    bool killed = false;
    bool incapacitated = false;
    bool stateChanged = false;
    bool armorHit = false;
    bool armorPenetrated = false;
    std::uint32_t targetId = 0;
    HitRegion region = HitRegion::None;
    ArmorZone armorZone = ArmorZone::None;
    DamageCause cause = DamageCause::Impact;
    CombatState previousState = CombatState::Effective;
    CombatState newState = CombatState::Effective;
    double damage = 0.0;
    double armorAbsorbedJoules = 0.0;
    double bleedingAddedPerSecond = 0.0;
    double remainingHealth = 0.0;
    double bleedingPerSecond = 0.0;
    Vec3 reactionDirection{};
    std::uint64_t correlationId = 0;
};

struct DamageMetrics {
    std::uint64_t hits = 0;
    std::uint64_t kills = 0;
    std::uint64_t incapacitations = 0;
    std::uint64_t armorHits = 0;
    std::uint64_t bleedTransitions = 0;
};

class DamageCore final {
public:
    static constexpr std::size_t kMaxTargets = 32;
    // One simulation slice can receive at most 128 projectile contacts plus at most
    // one bleeding state transition per target. Keep enough bounded history to publish
    // every same-slice result atomically without dynamic allocation.
    static constexpr std::size_t kResultCapacity = 192;
    static constexpr double kMaxBleedingPerSecond = 4.0;

    DamageCore() noexcept;
    void reset() noexcept;
    void fixedStep(double dt) noexcept;
    [[nodiscard]] DamageIntersection traceSegment(const Vec3& from,const Vec3& to) const noexcept;
    DamageResult applyIntersection(const DamageIntersection& hit,
                                   double projectileEnergyJ,
                                   std::uint64_t correlationId,
                                   Vec3 impactDirection = {}) noexcept;
    DamageResult applySegment(const Vec3& from,
                              const Vec3& to,
                              double projectileEnergyJ,
                              std::uint64_t correlationId) noexcept;
    [[nodiscard]] const std::array<DamageTarget,kMaxTargets>& targets() const noexcept { return targets_; }
    [[nodiscard]] std::size_t targetCount() const noexcept { return targetCount_; }
    [[nodiscard]] std::uint64_t totalHits() const noexcept { return metrics_.hits; }
    [[nodiscard]] std::uint64_t totalKills() const noexcept { return metrics_.kills; }
    [[nodiscard]] std::uint64_t totalIncapacitations() const noexcept { return metrics_.incapacitations; }
    [[nodiscard]] const DamageMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const DamageResult& lastResult() const noexcept { return lastResult_; }
    [[nodiscard]] std::uint64_t resultSequence() const noexcept { return resultSequence_; }
    [[nodiscard]] bool resultBySequence(std::uint64_t sequence,DamageResult& out) const noexcept;
    [[nodiscard]] static bool combatCapable(const DamageTarget& target) noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    static double segmentPointDistanceXZ(const Vec3& a,const Vec3& b,const Vec3& p,double& t) noexcept;
    static Vec3 normalize(Vec3 value) noexcept;
    static CombatState classifyState(double health,double bleedingPerSecond) noexcept;
    static ArmorZone armorZoneFor(HitRegion region) noexcept;
    static double regionDamageMultiplier(HitRegion region) noexcept;
    static double regionBleedingScale(HitRegion region) noexcept;
    void queueResult(DamageResult result) noexcept;
    void applyStateTransition(DamageTarget& target,
                              CombatState previous,
                              CombatState next,
                              DamageResult& result) noexcept;

    std::array<DamageTarget,kMaxTargets> targets_{};
    std::size_t targetCount_ = 0;
    DamageMetrics metrics_{};
    std::array<DamageResult,kResultCapacity> results_{};
    DamageResult lastResult_{};
    std::uint64_t resultSequence_ = 0;
};

} // namespace metse
