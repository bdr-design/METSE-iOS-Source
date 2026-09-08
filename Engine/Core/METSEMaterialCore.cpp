#include "METSEMaterialCore.hpp"
#include <cmath>

namespace metse {

MaterialBallisticProfile MaterialCore::ballistic(WorldMaterial material) noexcept {
    switch (material) {
        case WorldMaterial::Wood:
            return {material,420.0,0.34,0.58,true};
        case WorldMaterial::Steel:
            return {material,2200.0,0.0,0.0,false};
        case WorldMaterial::Concrete:
        default:
            return {WorldMaterial::Concrete,1650.0,0.0,0.0,false};
    }
}

bool MaterialCore::validateProfile(const MaterialBallisticProfile& profile) noexcept {
    if (!std::isfinite(profile.penetrationThresholdJoules) || profile.penetrationThresholdJoules < 0.0) return false;
    if (!std::isfinite(profile.retainedEnergyFraction) || profile.retainedEnergyFraction < 0.0 || profile.retainedEnergyFraction > 1.0) return false;
    if (!std::isfinite(profile.retainedVelocityFraction) || profile.retainedVelocityFraction < 0.0 || profile.retainedVelocityFraction > 1.0) return false;
    if (!profile.penetrable && (profile.retainedEnergyFraction != 0.0 || profile.retainedVelocityFraction != 0.0)) return false;
    if (profile.penetrable && (profile.retainedEnergyFraction <= 0.0 || profile.retainedVelocityFraction <= 0.0)) return false;
    return true;
}

} // namespace metse
