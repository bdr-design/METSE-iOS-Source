#include "METSEEngineCore.hpp"

#include <algorithm>
#include <cmath>

namespace metse {

EngineCore::EngineCore(EngineConfig config) : config_(config) {
    if (!std::isfinite(config_.fixedStepSeconds) || config_.fixedStepSeconds <= 0.0) {
        config_.fixedStepSeconds = 1.0 / 60.0;
    }
    config_.maxCatchUpSteps = std::clamp<std::uint32_t>(config_.maxCatchUpSteps, 1, 8);
    config_.maxCombatants = std::clamp<std::uint32_t>(config_.maxCombatants, 1, 32);
    if (!std::isfinite(config_.playerMoveSpeed) || config_.playerMoveSpeed <= 0.0) {
        config_.playerMoveSpeed = 4.5;
    }
    resetState();
    integrity_.appendSystemEvent(EventKind::EngineBoot, state_.simulationTick);
}

void EngineCore::resetState() noexcept {
    state_ = {};
    accumulatorSeconds_ = 0.0;
    moveForward_ = 0.0;
    moveStrafe_ = 0.0;
}

void EngineCore::reset() {
    executeAtomic(CommandKind::ResetSession, true, EventKind::SessionReset, [this] {
        resetState();
    });
}

bool EngineCore::setActiveCombatants(std::uint32_t count) {
    return executeAtomic(CommandKind::SetActiveCombatants,
                         count <= config_.maxCombatants,
                         EventKind::CombatantCountChanged,
                         [this, count] {
                             state_.activeCombatants = count;
                         });
}

void EngineCore::setMovementInput(double forward, double strafe) {
    const bool valid = std::isfinite(forward) && std::isfinite(strafe);
    executeAtomic(CommandKind::SetMovementIntent, valid, EventKind::MovementIntentChanged, [this, forward, strafe] {
        moveForward_ = std::clamp(forward, -1.0, 1.0);
        moveStrafe_ = std::clamp(strafe, -1.0, 1.0);
        const double length = std::hypot(moveForward_, moveStrafe_);
        if (length > 1.0) {
            moveForward_ /= length;
            moveStrafe_ /= length;
        }
    });
}

void EngineCore::addLookInput(double yawDeltaRadians, double pitchDeltaRadians) {
    const bool valid = std::isfinite(yawDeltaRadians) && std::isfinite(pitchDeltaRadians);
    executeAtomic(CommandKind::AddLookIntent, valid, EventKind::LookIntentChanged, [this, yawDeltaRadians, pitchDeltaRadians] {
        state_.playerYaw += std::clamp(yawDeltaRadians, -0.35, 0.35);
        state_.playerPitch = std::clamp(state_.playerPitch + std::clamp(pitchDeltaRadians, -0.25, 0.25), -1.15, 1.15);
        constexpr double twoPi = 6.28318530717958647692;
        state_.playerYaw = std::remainder(state_.playerYaw, twoPi);
    });
}

void EngineCore::triggerFire() {
    const bool canIncrement = state_.shotsFired != std::numeric_limits<std::uint64_t>::max();
    executeAtomic(CommandKind::FireWeapon, canIncrement, EventKind::ShotFired, [this] {
        ++state_.shotsFired;
    });
}

void EngineCore::advance(double realDeltaSeconds) {
    if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds <= 0.0) {
        state_.interpolationAlpha = std::clamp(accumulatorSeconds_ / config_.fixedStepSeconds, 0.0, 1.0);
        recordBlackBox(0.0, 0, false);
        return;
    }

    const double maxAcceptedDelta = config_.fixedStepSeconds * static_cast<double>(config_.maxCatchUpSteps);
    const bool inputClamped = realDeltaSeconds > maxAcceptedDelta;
    accumulatorSeconds_ += std::min(realDeltaSeconds, maxAcceptedDelta);

    std::uint32_t steps = 0;
    while (accumulatorSeconds_ >= config_.fixedStepSeconds && steps < config_.maxCatchUpSteps) {
        fixedStep();
        accumulatorSeconds_ -= config_.fixedStepSeconds;
        ++steps;
    }

    bool backlogClamped = false;
    if (steps == config_.maxCatchUpSteps && accumulatorSeconds_ >= config_.fixedStepSeconds) {
        accumulatorSeconds_ = std::fmod(accumulatorSeconds_, config_.fixedStepSeconds);
        backlogClamped = true;
    }

    state_.interpolationAlpha = std::clamp(accumulatorSeconds_ / config_.fixedStepSeconds, 0.0, 1.0);
    recordBlackBox(realDeltaSeconds, steps, inputClamped || backlogClamped);
}

void EngineCore::fixedStep() noexcept {
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

bool EngineCore::validateInvariants() const noexcept {
    const auto finite = [](double value) { return std::isfinite(value); };
    if (!finite(config_.fixedStepSeconds) || config_.fixedStepSeconds <= 0.0) return false;
    if (!finite(config_.playerMoveSpeed) || config_.playerMoveSpeed <= 0.0) return false;
    if (config_.maxCatchUpSteps < 1 || config_.maxCatchUpSteps > 8) return false;
    if (config_.maxCombatants < 1 || config_.maxCombatants > 32) return false;
    if (state_.activeCombatants > config_.maxCombatants) return false;
    if (!finite(state_.simulationSeconds) || state_.simulationSeconds < 0.0) return false;
    if (!finite(state_.interpolationAlpha) || state_.interpolationAlpha < 0.0 || state_.interpolationAlpha > 1.0) return false;
    if (!finite(state_.playerX) || !finite(state_.playerZ) || !finite(state_.playerYaw) || !finite(state_.playerPitch)) return false;
    if (state_.playerPitch < -1.15 || state_.playerPitch > 1.15) return false;
    if (!finite(moveForward_) || !finite(moveStrafe_)) return false;
    if (std::abs(moveForward_) > 1.0000001 || std::abs(moveStrafe_) > 1.0000001) return false;
    if (std::hypot(moveForward_, moveStrafe_) > 1.0000001) return false;
    return true;
}

void EngineCore::recordBlackBox(double realDeltaSeconds,
                                std::uint32_t catchUpSteps,
                                bool catchUpClamped) noexcept {
    BlackBoxFrame frame{};
    frame.simulationTick = state_.simulationTick;
    frame.simulationSeconds = state_.simulationSeconds;
    frame.realDeltaSeconds = realDeltaSeconds;
    frame.playerX = state_.playerX;
    frame.playerZ = state_.playerZ;
    frame.playerYaw = state_.playerYaw;
    frame.playerPitch = state_.playerPitch;
    frame.moveForward = moveForward_;
    frame.moveStrafe = moveStrafe_;
    frame.shotsFired = state_.shotsFired;
    frame.catchUpSteps = catchUpSteps;
    frame.catchUpClamped = catchUpClamped;

    blackBox_[blackBoxWrite_] = frame;
    blackBoxWrite_ = (blackBoxWrite_ + 1) % kBlackBoxCapacity;
    blackBoxCount_ = std::min(blackBoxCount_ + 1, kBlackBoxCapacity);
}

EngineDiagnostics EngineCore::diagnostics() const noexcept {
    EngineDiagnostics out{};
    out.integrity = integrity_.metrics();
    out.journalHead = integrity_.journalHead();
    out.retainedEvents = integrity_.eventCount();
    out.retainedCommands = integrity_.commandCount();
    out.retainedBlackBoxFrames = blackBoxCount_;
    out.journalValid = integrity_.verifyJournal();
    return out;
}

bool EngineCore::newestBlackBoxFrame(std::size_t offset, BlackBoxFrame& out) const noexcept {
    if (offset >= blackBoxCount_) return false;
    const std::size_t index = (blackBoxWrite_ + kBlackBoxCapacity - 1 - offset) % kBlackBoxCapacity;
    out = blackBox_[index];
    return true;
}

#ifdef METSE_TESTING
bool EngineCore::testOnlyExecuteInvariantViolation() {
    return executeAtomic(CommandKind::AddLookIntent, true, EventKind::LookIntentChanged, [this] {
        state_.playerX = std::numeric_limits<double>::quiet_NaN();
    });
}
#endif

} // namespace metse
