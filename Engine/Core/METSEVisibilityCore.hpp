#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class VisibilityTier:std::uint8_t { Full=0, Reduced=1, Minimal=2, Dormant=3 };

struct VisibilityEntity {
    std::uint32_t id=0;
    Vec3 position{};
    VisibilityTier tier=VisibilityTier::Dormant;
    bool alive=true;
};

struct VisibilityReport {
    std::uint32_t full=0;
    std::uint32_t reduced=0;
    std::uint32_t minimal=0;
    std::uint32_t dormant=0;
    std::uint32_t evaluated=0;
    std::uint32_t budgetDemotions=0;
};

class VisibilityCore final {
public:
    static constexpr std::size_t kMaxEntities=32;
    static constexpr std::uint32_t kFullBudget=8;
    static constexpr std::uint32_t kReducedBudget=12;
    static constexpr std::uint32_t kMinimalBudget=12;

    void reset() noexcept;
    void syncTarget(std::size_t index,std::uint32_t id,Vec3 position,bool alive) noexcept;
    void update(Vec3 camera,double yaw) noexcept;
    [[nodiscard]] VisibilityReport report() const noexcept;
    [[nodiscard]] const std::array<VisibilityEntity,kMaxEntities>& entities() const noexcept { return entities_; }
    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    std::array<VisibilityEntity,kMaxEntities> entities_{};
    std::size_t count_=0;
    std::uint32_t lastEvaluated_=0;
    std::uint32_t lastBudgetDemotions_=0;
};

} // namespace metse
