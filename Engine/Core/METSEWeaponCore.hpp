#pragma once
#include <cstdint>

namespace metse {

struct Vec3 { double x=0.0,y=0.0,z=0.0; };

struct WeaponConfig {
    std::uint32_t magazineSize = 30;
    std::uint32_t startingReserve = 90;
    double roundsPerMinute = 700.0;
    double reloadSeconds = 2.35;
    double adsTransitionSeconds = 0.18;
    double muzzleVelocity = 820.0;
    double projectileMassKg = 0.0040;
    double sightConvergenceMeters = 100.0;
};

struct WeaponState {
    std::uint32_t ammoInMagazine = 30;
    std::uint32_t reserveAmmo = 90;
    double fireCooldown = 0.0;
    double reloadRemaining = 0.0;
    double adsAlpha = 0.0;
    double recoilPitch = 0.0;
    double recoilYaw = 0.0;
    double swayX = 0.0;
    double swayY = 0.0;
    bool aimingHeld = false;
    bool reloading = false;
    bool obstructed = false;
};

struct ShotSolution {
    Vec3 origin{};
    Vec3 direction{0.0,0.0,1.0};
    double muzzleVelocity = 0.0;
    double massKg = 0.0;
    std::uint64_t correlationId = 0;
};

class WeaponCore final {
public:
    explicit WeaponCore(WeaponConfig config = {}) noexcept;
    void reset() noexcept;
    void setAimHeld(bool held) noexcept { state_.aimingHeld = held; }
    bool requestReload() noexcept;
    void fixedStep(double dt, double horizontalSpeed, double strafeInput) noexcept;
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
    WeaponConfig config_{};
    WeaponState state_{};
};

} // namespace metse
