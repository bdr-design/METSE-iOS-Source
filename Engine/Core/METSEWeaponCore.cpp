#include "METSEWeaponCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace {
bool positiveFinite(double v) noexcept { return std::isfinite(v) && v > 0.0; }
}

WeaponCore::WeaponCore(WeaponConfig config) noexcept : config_(config) {
    if (config_.magazineSize == 0 || config_.magazineSize > 200) config_.magazineSize = 30;
    if (config_.startingReserve > 1000) config_.startingReserve = 90;
    if (!positiveFinite(config_.roundsPerMinute)) config_.roundsPerMinute = 700.0;
    if (!positiveFinite(config_.reloadSeconds)) config_.reloadSeconds = 2.35;
    if (!positiveFinite(config_.adsTransitionSeconds)) config_.adsTransitionSeconds = 0.18;
    if (!positiveFinite(config_.muzzleVelocity)) config_.muzzleVelocity = 820.0;
    if (!positiveFinite(config_.projectileMassKg)) config_.projectileMassKg = 0.0040;
    if (!positiveFinite(config_.sightConvergenceMeters)) config_.sightConvergenceMeters = 100.0;
    reset();
}

void WeaponCore::reset() noexcept {
    state_ = {};
    state_.ammoInMagazine = config_.magazineSize;
    state_.reserveAmmo = config_.startingReserve;
}

bool WeaponCore::requestReload() noexcept {
    if (state_.reloading || state_.ammoInMagazine >= config_.magazineSize || state_.reserveAmmo == 0) return false;
    state_.reloading = true;
    state_.reloadRemaining = config_.reloadSeconds;
    return true;
}

void WeaponCore::fixedStep(double dt, double horizontalSpeed, double strafeInput) noexcept {
    if (!positiveFinite(dt) || !std::isfinite(horizontalSpeed) || !std::isfinite(strafeInput)) return;
    state_.fireCooldown = std::max(0.0, state_.fireCooldown - dt);
    const double adsTarget = (state_.aimingHeld && !state_.reloading) ? 1.0 : 0.0;
    const double adsRate = dt / config_.adsTransitionSeconds;
    state_.adsAlpha += std::clamp(adsTarget - state_.adsAlpha, -adsRate, adsRate);
    state_.adsAlpha = std::clamp(state_.adsAlpha, 0.0, 1.0);

    if (state_.reloading) {
        state_.reloadRemaining = std::max(0.0, state_.reloadRemaining - dt);
        if (state_.reloadRemaining <= 0.0) {
            const std::uint32_t needed = config_.magazineSize - state_.ammoInMagazine;
            const std::uint32_t transfer = std::min(needed, state_.reserveAmmo);
            state_.ammoInMagazine += transfer;
            state_.reserveAmmo -= transfer;
            state_.reloading = false;
        }
    }

    state_.recoilPitch *= std::max(0.0, 1.0 - dt * 10.0);
    state_.recoilYaw *= std::max(0.0, 1.0 - dt * 11.5);
    const double speedFactor = std::clamp(horizontalSpeed / 6.0, 0.0, 1.0);
    const double swayTargetX = std::sin(horizontalSpeed * 0.73 + state_.fireCooldown * 17.0) * 0.010 * speedFactor + std::clamp(strafeInput,-1.0,1.0)*0.006;
    const double swayTargetY = std::cos(horizontalSpeed * 0.51 + state_.fireCooldown * 13.0) * 0.007 * speedFactor;
    state_.swayX += (swayTargetX - state_.swayX) * std::min(1.0, dt * 9.0);
    state_.swayY += (swayTargetY - state_.swayY) * std::min(1.0, dt * 9.0);
}

bool WeaponCore::fire(const Vec3& cameraPosition,
                      double yaw,
                      double pitch,
                      std::uint64_t correlationId,
                      ShotSolution& out) noexcept {
    if (!std::isfinite(cameraPosition.x) || !std::isfinite(cameraPosition.y) || !std::isfinite(cameraPosition.z) ||
        !std::isfinite(yaw) || !std::isfinite(pitch) || state_.reloading || state_.obstructed ||
        state_.fireCooldown > 0.0 || state_.ammoInMagazine == 0 || correlationId == 0) return false;

    const double cp = std::cos(pitch + state_.recoilPitch * 0.25);
    const double sp = std::sin(pitch + state_.recoilPitch * 0.25);
    const double sy = std::sin(yaw + state_.recoilYaw * 0.22);
    const double cy = std::cos(yaw + state_.recoilYaw * 0.22);
    const Vec3 viewDirection = normalize({sy * cp, sp, cy * cp});
    const Vec3 right = {cy, 0.0, -sy};
    const Vec3 muzzle = add(add(add(cameraPosition, mul(viewDirection, 0.43)), mul(right, 0.115)), {0.0, -0.085, 0.0});
    const Vec3 aimPoint = add(cameraPosition, mul(viewDirection, config_.sightConvergenceMeters));
    out.origin = muzzle;
    out.direction = normalize({aimPoint.x-muzzle.x, aimPoint.y-muzzle.y, aimPoint.z-muzzle.z});
    out.muzzleVelocity = config_.muzzleVelocity;
    out.massKg = config_.projectileMassKg;
    out.correlationId = correlationId;

    --state_.ammoInMagazine;
    state_.fireCooldown = 60.0 / config_.roundsPerMinute;
    state_.recoilPitch = std::min(0.095, state_.recoilPitch + 0.028 * (1.0 - 0.45 * state_.adsAlpha));
    const double direction = (correlationId & 1u) ? 1.0 : -1.0;
    state_.recoilYaw = std::clamp(state_.recoilYaw + direction * 0.009, -0.05, 0.05);
    return true;
}

Vec3 WeaponCore::normalize(Vec3 v) noexcept {
    const double length = std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
    if (!positiveFinite(length)) return {0.0,0.0,1.0};
    return {v.x/length,v.y/length,v.z/length};
}
Vec3 WeaponCore::add(Vec3 a, Vec3 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 WeaponCore::mul(Vec3 a, double s) noexcept { return {a.x*s,a.y*s,a.z*s}; }

bool WeaponCore::validate() const noexcept {
    if (state_.ammoInMagazine > config_.magazineSize || state_.reserveAmmo > 1000) return false;
    if (!std::isfinite(state_.fireCooldown) || state_.fireCooldown < 0.0) return false;
    if (!std::isfinite(state_.reloadRemaining) || state_.reloadRemaining < 0.0) return false;
    if (!std::isfinite(state_.adsAlpha) || state_.adsAlpha < -1e-9 || state_.adsAlpha > 1.0000001) return false;
    if (!std::isfinite(state_.recoilPitch) || !std::isfinite(state_.recoilYaw) || !std::isfinite(state_.swayX) || !std::isfinite(state_.swayY)) return false;
    if (state_.reloading != (state_.reloadRemaining > 0.0)) return false;
    return true;
}

} // namespace metse
