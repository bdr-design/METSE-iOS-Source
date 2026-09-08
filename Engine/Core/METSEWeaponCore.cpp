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
    if (!positiveFinite(config_.emptyReloadSeconds) || config_.emptyReloadSeconds < config_.reloadSeconds) config_.emptyReloadSeconds = 2.70;
    if (!positiveFinite(config_.adsTransitionSeconds)) config_.adsTransitionSeconds = 0.18;
    if (!positiveFinite(config_.sprintToFireSeconds)) config_.sprintToFireSeconds = 0.16;
    if (!positiveFinite(config_.muzzleVelocity)) config_.muzzleVelocity = 820.0;
    if (!positiveFinite(config_.projectileMassKg)) config_.projectileMassKg = 0.0040;
    if (!positiveFinite(config_.sightConvergenceMeters)) config_.sightConvergenceMeters = 100.0;
    if (!positiveFinite(config_.recoilPitchImpulse)) config_.recoilPitchImpulse = 0.028;
    if (!positiveFinite(config_.recoilYawImpulse)) config_.recoilYawImpulse = 0.009;
    if (!positiveFinite(config_.recoilPitchRecoveryPerSecond)) config_.recoilPitchRecoveryPerSecond = 10.0;
    if (!positiveFinite(config_.recoilYawRecoveryPerSecond)) config_.recoilYawRecoveryPerSecond = 11.5;
    if (!positiveFinite(config_.movementSwayScale)) config_.movementSwayScale = 1.0;
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
    state_.reloadKind = state_.ammoInMagazine == 0 ? ReloadKind::Empty : ReloadKind::Tactical;
    state_.reloadRemaining = state_.reloadKind == ReloadKind::Empty ? config_.emptyReloadSeconds : config_.reloadSeconds;
    return true;
}

void WeaponCore::fixedStep(double dt, double horizontalSpeed, double strafeInput, bool sprinting) noexcept {
    if (!positiveFinite(dt) || !std::isfinite(horizontalSpeed) || !std::isfinite(strafeInput)) return;
    state_.fireCooldown = std::max(0.0, state_.fireCooldown - dt);
    state_.sprinting = sprinting;
    if (sprinting) state_.sprintRecoveryRemaining = config_.sprintToFireSeconds;
    else state_.sprintRecoveryRemaining = std::max(0.0, state_.sprintRecoveryRemaining - dt);

    const double adsTarget = (state_.aimingHeld && !state_.reloading && !state_.sprinting) ? 1.0 : 0.0;
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
            state_.reloadKind = ReloadKind::None;
        }
    }

    state_.recoilPitch *= std::max(0.0, 1.0 - dt * config_.recoilPitchRecoveryPerSecond);
    state_.recoilYaw *= std::max(0.0, 1.0 - dt * config_.recoilYawRecoveryPerSecond);
    const double speedFactor = std::clamp(horizontalSpeed / 6.0, 0.0, 1.0);
    const double swayScale = config_.movementSwayScale * (1.0 - 0.52 * state_.adsAlpha);
    const double swayTargetX = (std::sin(horizontalSpeed * 0.73 + state_.fireCooldown * 17.0) * 0.010 * speedFactor + std::clamp(strafeInput,-1.0,1.0)*0.006) * swayScale;
    const double swayTargetY = std::cos(horizontalSpeed * 0.51 + state_.fireCooldown * 13.0) * 0.007 * speedFactor * swayScale;
    state_.swayX += (swayTargetX - state_.swayX) * std::min(1.0, dt * 9.0);
    state_.swayY += (swayTargetY - state_.swayY) * std::min(1.0, dt * 9.0);
}

bool WeaponCore::canFireNow() const noexcept {
    return !state_.reloading && !state_.obstructed && !state_.sprinting && state_.sprintRecoveryRemaining <= 0.0 && state_.fireCooldown <= 0.0 && state_.ammoInMagazine > 0;
}

Vec3 WeaponCore::viewDirection(double yaw,double pitch) const noexcept {
    if (!std::isfinite(yaw) || !std::isfinite(pitch)) return {0.0,0.0,1.0};
    const double cp=std::cos(pitch),sp=std::sin(pitch),sy=std::sin(yaw),cy=std::cos(yaw);
    return normalize({sy*cp,sp,cy*cp});
}

Vec3 WeaponCore::muzzlePosition(const Vec3& cameraPosition,double yaw,double pitch) const noexcept {
    const Vec3 forward=viewDirection(yaw,pitch);
    const double sy=std::sin(yaw),cy=std::cos(yaw);
    const Vec3 right={cy,0.0,-sy};
    return add(add(add(cameraPosition,mul(forward,0.43)),mul(right,0.115)),{0.0,-0.085,0.0});
}

bool WeaponCore::previewShot(const Vec3& cameraPosition,double yaw,double pitch,std::uint64_t correlationId,ShotSolution& out) const noexcept {
    if (!std::isfinite(cameraPosition.x)||!std::isfinite(cameraPosition.y)||!std::isfinite(cameraPosition.z)||!std::isfinite(yaw)||!std::isfinite(pitch)||correlationId==0) return false;
    const Vec3 forward=viewDirection(yaw,pitch);
    const Vec3 muzzle=muzzlePosition(cameraPosition,yaw,pitch);
    const Vec3 aimPoint=add(cameraPosition,mul(forward,config_.sightConvergenceMeters));
    out.origin=muzzle;
    out.aimPoint=aimPoint;
    out.direction=normalize({aimPoint.x-muzzle.x,aimPoint.y-muzzle.y,aimPoint.z-muzzle.z});
    out.muzzleVelocity=config_.muzzleVelocity;
    out.massKg=config_.projectileMassKg;
    out.correlationId=correlationId;
    return true;
}

bool WeaponCore::fire(const Vec3& cameraPosition,
                      double yaw,
                      double pitch,
                      std::uint64_t correlationId,
                      ShotSolution& out) noexcept {
    if (!canFireNow()) return false;
    if (!previewShot(cameraPosition,yaw,pitch,correlationId,out)) return false;

    --state_.ammoInMagazine;
    ++state_.shotSequence;
    state_.fireCooldown = 60.0 / config_.roundsPerMinute;
    const double adsImpulseScale = 1.0 - 0.45 * state_.adsAlpha;
    state_.recoilPitch = std::min(0.095, state_.recoilPitch + config_.recoilPitchImpulse * adsImpulseScale);
    state_.recoilYaw = std::clamp(state_.recoilYaw + recoilPattern(state_.shotSequence) * config_.recoilYawImpulse * adsImpulseScale, -0.05, 0.05);
    return true;
}

double WeaponCore::recoilPattern(std::uint64_t shotIndex) noexcept {
    // Fixed deterministic pattern: readable weapon personality without random aim error.
    static constexpr double pattern[8] = {-0.45,0.30,-0.18,0.55,-0.25,0.18,-0.38,0.42};
    return pattern[(shotIndex-1u) % 8u];
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
    if (!std::isfinite(state_.sprintRecoveryRemaining) || state_.sprintRecoveryRemaining < 0.0 || state_.sprintRecoveryRemaining > config_.sprintToFireSeconds + 1e-6) return false;
    if (!std::isfinite(state_.adsAlpha) || state_.adsAlpha < -1e-9 || state_.adsAlpha > 1.0000001) return false;
    if (!std::isfinite(state_.recoilPitch) || !std::isfinite(state_.recoilYaw) || !std::isfinite(state_.swayX) || !std::isfinite(state_.swayY)) return false;
    if (state_.reloading != (state_.reloadRemaining > 0.0)) return false;
    if (!state_.reloading && state_.reloadKind != ReloadKind::None) return false;
    if (state_.reloading && state_.reloadKind == ReloadKind::None) return false;
    return true;
}

} // namespace metse
