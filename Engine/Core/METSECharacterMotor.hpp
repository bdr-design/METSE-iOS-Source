#pragma once

#include <cstdint>

namespace metse {

enum class CharacterStance : std::uint8_t {
    Standing = 0,
    Crouched = 1,
    Prone = 2
};

struct CharacterConfig {
    double jogSpeed = 4.2;
    double sprintSpeed = 6.4;
    double crouchSpeed = 2.1;
    double proneSpeed = 0.85;
    double backwardMultiplier = 0.78;
    double groundAcceleration = 16.0;
    double groundDeceleration = 20.0;
    double airAcceleration = 3.0;
    double gravity = 18.0;
    double bodyTurnRate = 3.6;
    double viewYawSoftLimit = 0.60;
    double viewYawHardLimit = 1.22;
    double maxPitch = 1.10;
    double standingEyeHeight = 1.64;
    double crouchedEyeHeight = 1.08;
    double proneEyeHeight = 0.42;
    double eyeHeightTransitionSpeed = 3.0;
};

struct CharacterInput {
    double forward = 0.0;
    double strafe = 0.0;
    bool sprintHeld = false;
};

struct CharacterState {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    double bodyYaw = 0.0;
    double viewYawOffset = 0.0;
    double pitch = 0.0;
    double eyeHeight = 1.64;
    CharacterStance stance = CharacterStance::Standing;
    bool grounded = true;
    bool sprinting = false;
};

class CharacterMotor final {
public:
    explicit CharacterMotor(CharacterConfig config = {}) noexcept;

    void reset() noexcept;
    void addLookInput(double yawDeltaRadians, double pitchDeltaRadians) noexcept;
    void cycleStance() noexcept;
    void fixedStep(double dt, const CharacterInput& input) noexcept;

    [[nodiscard]] const CharacterConfig& config() const noexcept { return config_; }
    [[nodiscard]] const CharacterState& state() const noexcept { return state_; }
    [[nodiscard]] double cameraYaw() const noexcept;
    [[nodiscard]] double horizontalSpeed() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

#ifdef METSE_TESTING
    void testOnlySetAirborne(double heightMeters, double verticalVelocity) noexcept;
#endif

private:
    static double moveToward(double current, double target, double maxDelta) noexcept;
    static double wrapAngle(double radians) noexcept;
    double targetEyeHeight() const noexcept;

    CharacterConfig config_{};
    CharacterState state_{};
};

} // namespace metse
