#include "METSECharacterMotor.hpp"

#include <algorithm>
#include <cmath>

namespace metse {
namespace {

bool positiveFinite(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

double signOf(double value) noexcept {
    return value < 0.0 ? -1.0 : 1.0;
}

} // namespace

CharacterMotor::CharacterMotor(CharacterConfig config) noexcept : config_(config) {
    if (!positiveFinite(config_.jogSpeed)) config_.jogSpeed = 4.2;
    if (!positiveFinite(config_.sprintSpeed)) config_.sprintSpeed = 6.4;
    if (config_.sprintSpeed < config_.jogSpeed) config_.sprintSpeed = config_.jogSpeed;
    if (!positiveFinite(config_.crouchSpeed)) config_.crouchSpeed = 2.1;
    if (!positiveFinite(config_.proneSpeed)) config_.proneSpeed = 0.85;
    if (!positiveFinite(config_.backwardMultiplier) || config_.backwardMultiplier > 1.0) config_.backwardMultiplier = 0.78;
    if (!positiveFinite(config_.groundAcceleration)) config_.groundAcceleration = 16.0;
    if (!positiveFinite(config_.groundDeceleration)) config_.groundDeceleration = 20.0;
    if (!positiveFinite(config_.airAcceleration)) config_.airAcceleration = 3.0;
    if (!positiveFinite(config_.gravity)) config_.gravity = 18.0;
    if (!positiveFinite(config_.bodyTurnRate)) config_.bodyTurnRate = 3.6;
    if (!positiveFinite(config_.viewYawSoftLimit)) config_.viewYawSoftLimit = 0.60;
    if (!positiveFinite(config_.viewYawHardLimit) || config_.viewYawHardLimit <= config_.viewYawSoftLimit) config_.viewYawHardLimit = 1.22;
    if (!positiveFinite(config_.maxPitch) || config_.maxPitch > 1.45) config_.maxPitch = 1.10;
    if (!positiveFinite(config_.standingEyeHeight)) config_.standingEyeHeight = 1.64;
    if (!positiveFinite(config_.crouchedEyeHeight) || config_.crouchedEyeHeight >= config_.standingEyeHeight) config_.crouchedEyeHeight = 1.08;
    if (!positiveFinite(config_.proneEyeHeight) || config_.proneEyeHeight >= config_.crouchedEyeHeight) config_.proneEyeHeight = 0.42;
    if (!positiveFinite(config_.eyeHeightTransitionSpeed)) config_.eyeHeightTransitionSpeed = 3.0;
    reset();
}

void CharacterMotor::reset() noexcept {
    state_ = {};
    state_.eyeHeight = config_.standingEyeHeight;
    state_.grounded = true;
    state_.stance = CharacterStance::Standing;
}

void CharacterMotor::addLookInput(double yawDeltaRadians, double pitchDeltaRadians) noexcept {
    const double yawDelta = std::clamp(yawDeltaRadians, -0.35, 0.35);
    const double pitchDelta = std::clamp(pitchDeltaRadians, -0.25, 0.25);
    state_.viewYawOffset = std::clamp(state_.viewYawOffset + yawDelta,
                                      -config_.viewYawHardLimit,
                                      config_.viewYawHardLimit);
    state_.pitch = std::clamp(state_.pitch + pitchDelta, -config_.maxPitch, config_.maxPitch);
}

void CharacterMotor::cycleStance() noexcept {
    switch (state_.stance) {
        case CharacterStance::Standing: state_.stance = CharacterStance::Crouched; break;
        case CharacterStance::Crouched: state_.stance = CharacterStance::Prone; break;
        case CharacterStance::Prone: state_.stance = CharacterStance::Standing; break;
    }
    state_.sprinting = false;
}

void CharacterMotor::fixedStep(double dt, const CharacterInput& rawInput) noexcept {
    if (!positiveFinite(dt)) return;

    double forward = std::clamp(rawInput.forward, -1.0, 1.0);
    double strafe = std::clamp(rawInput.strafe, -1.0, 1.0);
    double magnitude = std::hypot(forward, strafe);
    if (magnitude > 1.0) {
        forward /= magnitude;
        strafe /= magnitude;
        magnitude = 1.0;
    }

    const double excessYaw = std::abs(state_.viewYawOffset) - config_.viewYawSoftLimit;
    if (excessYaw > 0.0) {
        const double turn = std::min(excessYaw, config_.bodyTurnRate * dt);
        const double signedTurn = signOf(state_.viewYawOffset) * turn;
        state_.bodyYaw = wrapAngle(state_.bodyYaw + signedTurn);
        state_.viewYawOffset -= signedTurn;
    }

    double speedLimit = 0.0;
    const bool hasMovement = magnitude > 0.0001;
    switch (state_.stance) {
        case CharacterStance::Standing:
            state_.sprinting = rawInput.sprintHeld && forward > 0.35 && magnitude > 0.25;
            speedLimit = state_.sprinting ? config_.sprintSpeed : config_.jogSpeed;
            break;
        case CharacterStance::Crouched:
            state_.sprinting = false;
            speedLimit = config_.crouchSpeed;
            break;
        case CharacterStance::Prone:
            state_.sprinting = false;
            speedLimit = config_.proneSpeed;
            break;
    }
    if (forward < -0.05) speedLimit *= config_.backwardMultiplier;

    const double s = std::sin(state_.bodyYaw);
    const double c = std::cos(state_.bodyYaw);
    const double targetVelocityX = hasMovement ? (s * forward + c * strafe) * speedLimit * magnitude : 0.0;
    const double targetVelocityZ = hasMovement ? (c * forward - s * strafe) * speedLimit * magnitude : 0.0;
    const double response = state_.grounded
        ? (hasMovement ? config_.groundAcceleration : config_.groundDeceleration)
        : config_.airAcceleration;

    state_.velocityX = moveToward(state_.velocityX, targetVelocityX, response * dt);
    state_.velocityZ = moveToward(state_.velocityZ, targetVelocityZ, response * dt);

    if (!state_.grounded || state_.y > 0.000001 || state_.velocityY > 0.0) {
        state_.grounded = false;
        state_.velocityY -= config_.gravity * dt;
        state_.y += state_.velocityY * dt;
        if (state_.y <= 0.0) {
            state_.y = 0.0;
            state_.velocityY = 0.0;
            state_.grounded = true;
        }
    } else {
        state_.y = 0.0;
        state_.velocityY = 0.0;
        state_.grounded = true;
    }

    state_.x += state_.velocityX * dt;
    state_.z += state_.velocityZ * dt;
    state_.eyeHeight = moveToward(state_.eyeHeight,
                                  targetEyeHeight(),
                                  config_.eyeHeightTransitionSpeed * dt);
}

double CharacterMotor::cameraYaw() const noexcept {
    return wrapAngle(state_.bodyYaw + state_.viewYawOffset);
}

double CharacterMotor::horizontalSpeed() const noexcept {
    return std::hypot(state_.velocityX, state_.velocityZ);
}

bool CharacterMotor::validate() const noexcept {
    const auto finite = [](double v) { return std::isfinite(v); };
    if (!finite(state_.x) || !finite(state_.y) || !finite(state_.z)) return false;
    if (!finite(state_.velocityX) || !finite(state_.velocityY) || !finite(state_.velocityZ)) return false;
    if (!finite(state_.bodyYaw) || !finite(state_.viewYawOffset) || !finite(state_.pitch)) return false;
    if (!finite(state_.eyeHeight) || state_.eyeHeight <= 0.0) return false;
    if (state_.y < -0.000001) return false;
    if (std::abs(state_.viewYawOffset) > config_.viewYawHardLimit + 0.000001) return false;
    if (std::abs(state_.pitch) > config_.maxPitch + 0.000001) return false;
    if (state_.sprinting && state_.stance != CharacterStance::Standing) return false;
    const auto stanceValue = static_cast<std::uint8_t>(state_.stance);
    if (stanceValue > static_cast<std::uint8_t>(CharacterStance::Prone)) return false;
    return true;
}

#ifdef METSE_TESTING
void CharacterMotor::testOnlySetAirborne(double heightMeters, double verticalVelocity) noexcept {
    if (!std::isfinite(heightMeters) || !std::isfinite(verticalVelocity)) return;
    state_.y = std::max(0.0, heightMeters);
    state_.velocityY = verticalVelocity;
    state_.grounded = state_.y <= 0.0 && state_.velocityY <= 0.0;
}
#endif

double CharacterMotor::moveToward(double current, double target, double maxDelta) noexcept {
    if (current < target) return std::min(current + maxDelta, target);
    if (current > target) return std::max(current - maxDelta, target);
    return target;
}

double CharacterMotor::wrapAngle(double radians) noexcept {
    constexpr double twoPi = 6.28318530717958647692;
    return std::remainder(radians, twoPi);
}

double CharacterMotor::targetEyeHeight() const noexcept {
    switch (state_.stance) {
        case CharacterStance::Standing: return config_.standingEyeHeight;
        case CharacterStance::Crouched: return config_.crouchedEyeHeight;
        case CharacterStance::Prone: return config_.proneEyeHeight;
    }
    return config_.standingEyeHeight;
}

} // namespace metse
