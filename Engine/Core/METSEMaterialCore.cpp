#include "METSEMaterialCore.hpp"
#include <cmath>

namespace metse {

MaterialBallisticProfile MaterialCore::ballistic(WorldMaterial material) noexcept {
    switch (material) {
        case WorldMaterial::Wood:
            return {material,420.0,0.3364,0.58,0.22,0.0,0.0,0.0,true,false};
        case WorldMaterial::Brick:
            return {material,900.0,0.1296,0.36,0.14,700.0,0.16,0.28,true,true};
        case WorldMaterial::Glass:
            return {material,40.0,0.8464,0.92,0.04,0.0,0.0,0.0,true,false};
        case WorldMaterial::Steel:
            return {material,2200.0,0.0,0.0,0.0,700.0,0.32,0.42,false,true};
        case WorldMaterial::Rock:
            return {material,1900.0,0.0,0.0,0.0,650.0,0.20,0.34,false,true};
        case WorldMaterial::Soil:
            return {material,1200.0,0.0,0.0,0.0,0.0,0.0,0.0,false,false};
        case WorldMaterial::Concrete:
        default:
            return {WorldMaterial::Concrete,1650.0,0.0,0.0,0.0,600.0,0.22,0.38,false,true};
    }
}

bool MaterialCore::validateProfile(const MaterialBallisticProfile& profile) noexcept {
    if (static_cast<std::uint8_t>(profile.material) > static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
    if (!std::isfinite(profile.penetrationThresholdJoules) || profile.penetrationThresholdJoules < 0.0) return false;
    if (!std::isfinite(profile.retainedEnergyFraction) || profile.retainedEnergyFraction < 0.0 || profile.retainedEnergyFraction > 1.0) return false;
    if (!std::isfinite(profile.retainedVelocityFraction) || profile.retainedVelocityFraction < 0.0 || profile.retainedVelocityFraction > 1.0) return false;
    if (!std::isfinite(profile.maxPenetrationThicknessMeters) || profile.maxPenetrationThicknessMeters < 0.0) return false;
    if (!std::isfinite(profile.ricochetMinEnergyJoules) || profile.ricochetMinEnergyJoules < 0.0) return false;
    if (!std::isfinite(profile.ricochetMaxNormalCosine) || profile.ricochetMaxNormalCosine < 0.0 || profile.ricochetMaxNormalCosine > 1.0) return false;
    if (!std::isfinite(profile.ricochetRetainedVelocityFraction) || profile.ricochetRetainedVelocityFraction < 0.0 || profile.ricochetRetainedVelocityFraction > 1.0) return false;
    if (!profile.penetrable && (profile.retainedEnergyFraction != 0.0 || profile.retainedVelocityFraction != 0.0 || profile.maxPenetrationThicknessMeters != 0.0)) return false;
    if (profile.penetrable && (profile.penetrationThresholdJoules <= 0.0 || profile.retainedEnergyFraction <= 0.0 || profile.retainedVelocityFraction <= 0.0 || profile.maxPenetrationThicknessMeters <= 0.0)) return false;
    if (!profile.ricochetEligible && (profile.ricochetMinEnergyJoules != 0.0 || profile.ricochetMaxNormalCosine != 0.0 || profile.ricochetRetainedVelocityFraction != 0.0)) return false;
    if (profile.ricochetEligible && (profile.ricochetMinEnergyJoules <= 0.0 || profile.ricochetMaxNormalCosine <= 0.0 || profile.ricochetRetainedVelocityFraction <= 0.0)) return false;
    return true;
}

} // namespace metse
