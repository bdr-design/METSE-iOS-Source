#pragma once
#include "METSEWorldCollision.hpp"
#include <cstdint>

namespace metse {

struct MaterialBallisticProfile {
    WorldMaterial material = WorldMaterial::Concrete;
    double penetrationThresholdJoules = 0.0;
    double retainedEnergyFraction = 0.0;
    double retainedVelocityFraction = 0.0;
    double maxPenetrationThicknessMeters = 0.0;
    double ricochetMinEnergyJoules = 0.0;
    double ricochetMaxNormalCosine = 0.0; // lower cosine = more glancing impact required
    double ricochetRetainedVelocityFraction = 0.0;
    bool penetrable = false;
    bool ricochetEligible = false;
};

class MaterialCore final {
public:
    [[nodiscard]] static MaterialBallisticProfile ballistic(WorldMaterial material) noexcept;
    [[nodiscard]] static bool validateProfile(const MaterialBallisticProfile& profile) noexcept;
};

} // namespace metse
