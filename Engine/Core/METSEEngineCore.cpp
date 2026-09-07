#include "METSEEngineCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {

EngineCore::EngineCore(EngineConfig config) : config_(config) {
    if (!std::isfinite(config_.fixedStepSeconds) || config_.fixedStepSeconds <= 0.0) config_.fixedStepSeconds = 1.0 / 60.0;
    config_.maxCatchUpSteps = std::clamp<std::uint32_t>(config_.maxCatchUpSteps, 1, 8);
    config_.maxCombatants = std::clamp<std::uint32_t>(config_.maxCombatants, 1, 32);
    if (!std::isfinite(config_.playerMoveSpeed) || config_.playerMoveSpeed <= 0.0) config_.playerMoveSpeed = 4.5;
    reset();
}

void EngineCore::reset() {
    state_ = {};
    accumulatorSeconds_ = 0.0;
    moveForward_ = 0.0;
    moveStrafe_ = 0.0;
}

bool EngineCore::setActiveCombatants(std::uint32_t count) {
    if (count > config_.maxCombatants) return false;
    state_.activeCombatants = count;
    return true;
}

void EngineCore::setMovementInput(double forward, double strafe) {
    moveForward_ = std::clamp(std::isfinite(forward) ? forward : 0.0, -1.0, 1.0);
    moveStrafe_ = std::clamp(std::isfinite(strafe) ? strafe : 0.0, -1.0, 1.0);
    const double length = std::hypot(moveForward_, moveStrafe_);
    if (length > 1.0) { moveForward_ /= length; moveStrafe_ /= length; }
}

void EngineCore::addLookInput(double yawDeltaRadians, double pitchDeltaRadians) {
    if (std::isfinite(yawDeltaRadians)) state_.playerYaw += std::clamp(yawDeltaRadians, -0.35, 0.35);
    if (std::isfinite(pitchDeltaRadians)) state_.playerPitch = std::clamp(state_.playerPitch + std::clamp(pitchDeltaRadians, -0.25, 0.25), -1.15, 1.15);
    constexpr double twoPi = 6.28318530717958647692;
    state_.playerYaw = std::remainder(state_.playerYaw, twoPi);
}

void EngineCore::triggerFire() { ++state_.shotsFired; }

void EngineCore::advance(double realDeltaSeconds) {
    if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds <= 0.0) {
        state_.interpolationAlpha = accumulatorSeconds_ / config_.fixedStepSeconds;
        return;
    }
    const double maxAcceptedDelta = config_.fixedStepSeconds * static_cast<double>(config_.maxCatchUpSteps);
    accumulatorSeconds_ += std::min(realDeltaSeconds, maxAcceptedDelta);
    std::uint32_t steps = 0;
    while (accumulatorSeconds_ >= config_.fixedStepSeconds && steps < config_.maxCatchUpSteps) {
        fixedStep();
        accumulatorSeconds_ -= config_.fixedStepSeconds;
        ++steps;
    }
    if (steps == config_.maxCatchUpSteps && accumulatorSeconds_ >= config_.fixedStepSeconds) accumulatorSeconds_ = std::fmod(accumulatorSeconds_, config_.fixedStepSeconds);
    state_.interpolationAlpha = std::clamp(accumulatorSeconds_ / config_.fixedStepSeconds, 0.0, 1.0);
}

void EngineCore::fixedStep() {
    const double dt = config_.fixedStepSeconds;
    const double s = std::sin(state_.playerYaw);
    const double c = std::cos(state_.playerYaw);
    const double vx = (s * moveForward_ + c * moveStrafe_) * config_.playerMoveSpeed;
    const double vz = (c * moveForward_ - s * moveStrafe_) * config_.playerMoveSpeed;
    state_.playerX += vx * dt;
    state_.playerZ += vz * dt;
    state_.simulationSeconds += dt;
    ++state_.simulationTick;
}

} // namespace metse
