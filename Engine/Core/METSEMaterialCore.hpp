#pragma once
#include "METSEWorldCollision.hpp"
#include <cstdint>

namespace metse {

struct MaterialBallisticProfile {
    WorldMaterial material = WorldMaterial::Concrete;
    double penetrationThresholdJoules = 0.0;
    double retainedEnergyFraction = 0.0;
    double retainedVelocityFraction = 0.0;
    bool penetrable = false;
};

class MaterialCore final {
public:
    [[nodiscard]] static MaterialBallisticProfile ballistic(WorldMaterial material) noexcept;
    [[nodiscard]] static bool validateProfile(const MaterialBallisticProfile& profile) noexcept;
};

} // namespace metse
