#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class HitRegion : std::uint8_t { None=0, Head, Thorax, Abdomen, Limb };

struct DamageTarget {
    std::uint32_t id = 0;
    Vec3 position{};
    double health = 100.0;
    double radius = 0.34;
    bool alive = true;
};

struct DamageIntersection {
    bool hit = false;
    double t = 2.0;
    std::size_t targetIndex = 0;
    std::uint32_t targetId = 0;
    HitRegion region = HitRegion::None;
};

struct DamageResult {
    bool hit = false;
    bool killed = false;
    std::uint32_t targetId = 0;
    HitRegion region = HitRegion::None;
    double damage = 0.0;
    double remainingHealth = 0.0;
    std::uint64_t correlationId = 0;
};

class DamageCore final {
public:
    static constexpr std::size_t kMaxTargets = 32;
    DamageCore() noexcept;
    void reset() noexcept;
    [[nodiscard]] DamageIntersection traceSegment(const Vec3& from,const Vec3& to) const noexcept;
    DamageResult applyIntersection(const DamageIntersection& hit,double projectileEnergyJ,std::uint64_t correlationId) noexcept;
    DamageResult applySegment(const Vec3& from, const Vec3& to, double projectileEnergyJ, std::uint64_t correlationId) noexcept;
    [[nodiscard]] const std::array<DamageTarget,kMaxTargets>& targets() const noexcept { return targets_; }
    [[nodiscard]] std::size_t targetCount() const noexcept { return targetCount_; }
    [[nodiscard]] std::uint64_t totalHits() const noexcept { return totalHits_; }
    [[nodiscard]] std::uint64_t totalKills() const noexcept { return totalKills_; }
    [[nodiscard]] const DamageResult& lastResult() const noexcept { return lastResult_; }
    [[nodiscard]] std::uint64_t resultSequence() const noexcept { return resultSequence_; }
    [[nodiscard]] bool validate() const noexcept;
private:
    static double segmentPointDistanceXZ(const Vec3& a,const Vec3& b,const Vec3& p,double& t) noexcept;
    std::array<DamageTarget,kMaxTargets> targets_{};
    std::size_t targetCount_ = 0;
    std::uint64_t totalHits_ = 0;
    std::uint64_t totalKills_ = 0;
    DamageResult lastResult_{};
    std::uint64_t resultSequence_ = 0;
};

} // namespace metse
