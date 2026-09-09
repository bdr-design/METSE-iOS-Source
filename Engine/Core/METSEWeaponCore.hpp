#pragma once
#include "METSECombatantCore.hpp"
#include <cstdint>

namespace metse {

struct Vec3 { double x=0.0,y=0.0,z=0.0; };

enum class ReloadKind : std::uint8_t { None=0, Tactical=1, Empty=2 };

struct WeaponConfig {
    std::uint32_t magazineSize = 30;
    std::uint32_t startingReserve = 90;
    double roundsPerMinute = 700.0;
    double reloadSeconds = 2.35;              // tactical reload baseline
    double emptyReloadSeconds = 2.70;         // empty magazine adds bolt/charging manipulation
    double adsTransitionSeconds = 0.18;
    double sprintToFireSeconds = 0.16;
    double muzzleVelocity = 820.0;
    double projectileMassKg = 0.0040;
    double sightConvergenceMeters = 100.0;
    double recoilPitchImpulse = 0.028;
    double recoilYawImpulse = 0.009;
    double recoilPitchRecoveryPerSecond = 10.0;
    double recoilYawRecoveryPerSecond = 11.5;
    double movementSwayScale = 1.0;
};

struct WeaponState {
    std::uint32_t ammoInMagazine = 30;
    std::uint32_t reserveAmmo = 90;
    std::uint64_t shotSequence = 0;
    double fireCooldown = 0.0;
    double reloadRemaining = 0.0;
    double sprintRecoveryRemaining = 0.0;
    double adsAlpha = 0.0;
    double recoilPitch = 0.0;
    double recoilYaw = 0.0;
    double swayX = 0.0;
    double swayY = 0.0;
    bool aimingHeld = false;
    bool reloading = false;
    bool obstructed = false;
    bool sprinting = false;
    ReloadKind reloadKind = ReloadKind::None;
};

struct ShotSolution {
    Vec3 origin{};
    Vec3 direction{0.0,0.0,1.0};
    Vec3 aimPoint{};
    double muzzleVelocity = 0.0;
    double massKg = 0.0;
    std::uint64_t correlationId = 0;
    // Provenance is attached by EngineCore after WeaponCore accepts the shot. The
    // weapon remains responsible only for handling and Aim Truth, never target policy.
    CombatantId sourceCombatantId = 0;
    TeamId sourceTeamId = 0;
    FactionId sourceFactionId = 0;
    TargetingPolicy targetingPolicy = TargetingPolicy::HostileOnly;
    bool includePlayerTarget = false;
};

class WeaponCore final {
public:
    explicit WeaponCore(WeaponConfig config = {}) noexcept;
    void reset() noexcept;
    void setAimHeld(bool held) noexcept { state_.aimingHeld = held; }
    bool requestReload() noexcept;
    void fixedStep(double dt, double horizontalSpeed, double strafeInput, bool sprinting = false) noexcept;
    [[nodiscard]] bool canFireNow() const noexcept;
    // Aim Truth contract: the center camera ray is authoritative. Muzzle parallax
    // converges onto that same ray at sightConvergenceMeters. Visual recoil/sway
    // never silently changes projectile direction.
    [[nodiscard]] Vec3 viewDirection(double yaw,double pitch) const noexcept;
    [[nodiscard]] Vec3 muzzlePosition(const Vec3& cameraPosition,double yaw,double pitch) const noexcept;
    [[nodiscard]] bool previewShot(const Vec3& cameraPosition,double yaw,double pitch,std::uint64_t correlationId,ShotSolution& out) const noexcept;
    bool fire(const Vec3& cameraPosition,
              double yaw,
              double pitch,
              std::uint64_t correlationId,
              ShotSolution& out) noexcept;
    void setObstructed(bool value) noexcept { state_.obstructed = value; }
    [[nodiscard]] const WeaponConfig& config() const noexcept { return config_; }
    [[nodiscard]] const WeaponState& state() const noexcept { return state_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    static Vec3 normalize(Vec3 v) noexcept;
    static Vec3 add(Vec3 a, Vec3 b) noexcept;
    static Vec3 mul(Vec3 a, double s) noexcept;
    static double recoilPattern(std::uint64_t shotIndex) noexcept;
    WeaponConfig config_{};
    WeaponState state_{};
};

} // namespace metse
