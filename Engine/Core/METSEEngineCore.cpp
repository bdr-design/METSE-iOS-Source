#include "METSEEngineCore.hpp"

#include <algorithm>
#include <cmath>

namespace metse {
namespace {

bool nearlyEqual(double a, double b, double epsilon = 1e-9) noexcept {
    return std::abs(a - b) <= epsilon;
}

} // namespace

EngineCore::EngineCore(EngineConfig config)
    : config_(config), characterMotor_(config.character) {
    if (!std::isfinite(config_.fixedStepSeconds) || config_.fixedStepSeconds <= 0.0) {
        config_.fixedStepSeconds = 1.0 / 60.0;
    }
    config_.maxCatchUpSteps = std::clamp<std::uint32_t>(config_.maxCatchUpSteps, 1, 8);
    config_.maxCombatants = std::clamp<std::uint32_t>(config_.maxCombatants, 1, 32);
    config_.character = characterMotor_.config();
    resetState();
    integrity_.appendSystemEvent(EventKind::EngineBoot, state_.simulationTick);
}

void EngineCore::resetState() noexcept {
    state_ = {};
    accumulatorSeconds_ = 0.0;
    moveForward_ = 0.0;
    moveStrafe_ = 0.0;
    sprintHeld_ = false;
    characterMotor_.reset();
    syncCharacterSnapshot();
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
                         [this, count] { state_.activeCombatants = count; });
}

void EngineCore::setMovementInput(double forward, double strafe) {
    const bool valid = std::isfinite(forward) && std::isfinite(strafe);
    executeAtomic(CommandKind::SetMovementIntent, valid, EventKind::MovementIntentChanged, [this, forward, strafe] {
        moveForward_ = std::clamp(forward, -1.0, 1.0);
        moveStrafe_ = std::clamp(strafe, -1.0, 1.0);
        const double length = std::hypot(moveForward_, moveStrafe_);
        if (length > 1.0) { moveForward_ /= length; moveStrafe_ /= length; }
    });
}

void EngineCore::addLookInput(double yawDeltaRadians, double pitchDeltaRadians) {
    const bool valid = std::isfinite(yawDeltaRadians) && std::isfinite(pitchDeltaRadians);
    executeAtomic(CommandKind::AddLookIntent, valid, EventKind::LookIntentChanged, [this, yawDeltaRadians, pitchDeltaRadians] {
        characterMotor_.addLookInput(yawDeltaRadians, pitchDeltaRadians);
    });
}

void EngineCore::setSprintHeld(bool held) {
    executeAtomic(CommandKind::SetSprintIntent, true, EventKind::SprintIntentChanged, [this, held] { sprintHeld_ = held; });
}

void EngineCore::cycleStance() {
    executeAtomic(CommandKind::CycleStance, true, EventKind::StanceChanged, [this] { characterMotor_.cycleStance(); });
}

void EngineCore::triggerFire() {
    const bool canIncrement = state_.shotsFired != std::numeric_limits<std::uint64_t>::max();
    executeAtomic(CommandKind::FireWeapon, canIncrement, EventKind::ShotFired, [this] { ++state_.shotsFired; });
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
    const EngineSnapshot stateCheckpoint = state_;
    const CharacterMotor characterCheckpoint = characterMotor_;

    CharacterInput input{};
    input.forward = moveForward_;
    input.strafe = moveStrafe_;
    input.sprintHeld = sprintHeld_;
    characterMotor_.fixedStep(config_.fixedStepSeconds, input);
    state_.simulationSeconds += config_.fixedStepSeconds;
    ++state_.simulationTick;
    syncCharacterSnapshot();

    if (!validateInvariants()) {
        state_ = stateCheckpoint;
        characterMotor_ = characterCheckpoint;
        integrity_.appendSystemEvent(EventKind::SimulationInvariantRolledBack, state_.simulationTick);
    }
}

void EngineCore::syncCharacterSnapshot() noexcept {
    const auto& character = characterMotor_.state();
    state_.playerX = character.x;
    state_.playerY = character.y;
    state_.playerZ = character.z;
    state_.velocityX = character.velocityX;
    state_.velocityY = character.velocityY;
    state_.velocityZ = character.velocityZ;
    state_.playerBodyYaw = character.bodyYaw;
    state_.playerYaw = characterMotor_.cameraYaw();
    state_.playerPitch = character.pitch;
    state_.cameraHeight = character.eyeHeight;
    state_.horizontalSpeed = characterMotor_.horizontalSpeed();
    state_.stance = character.stance;
    state_.grounded = character.grounded;
    state_.sprinting = character.sprinting;
}

bool EngineCore::validateInvariants() const noexcept {
    const auto finite = [](double value) { return std::isfinite(value); };
    if (!finite(config_.fixedStepSeconds) || config_.fixedStepSeconds <= 0.0) return false;
    if (config_.maxCatchUpSteps < 1 || config_.maxCatchUpSteps > 8) return false;
    if (config_.maxCombatants < 1 || config_.maxCombatants > 32) return false;
    if (state_.activeCombatants > config_.maxCombatants) return false;
    if (!finite(state_.simulationSeconds) || state_.simulationSeconds < 0.0) return false;
    if (!finite(state_.interpolationAlpha) || state_.interpolationAlpha < 0.0 || state_.interpolationAlpha > 1.0) return false;
    if (!finite(moveForward_) || !finite(moveStrafe_)) return false;
    if (std::abs(moveForward_) > 1.0000001 || std::abs(moveStrafe_) > 1.0000001) return false;
    if (std::hypot(moveForward_, moveStrafe_) > 1.0000001) return false;
    if (!characterMotor_.validate()) return false;

    const auto& character = characterMotor_.state();
    if (!nearlyEqual(state_.playerX, character.x) ||
        !nearlyEqual(state_.playerY, character.y) ||
        !nearlyEqual(state_.playerZ, character.z) ||
        !nearlyEqual(state_.velocityX, character.velocityX) ||
        !nearlyEqual(state_.velocityY, character.velocityY) ||
        !nearlyEqual(state_.velocityZ, character.velocityZ) ||
        !nearlyEqual(state_.playerBodyYaw, character.bodyYaw) ||
        !nearlyEqual(state_.playerYaw, characterMotor_.cameraYaw()) ||
        !nearlyEqual(state_.playerPitch, character.pitch) ||
        !nearlyEqual(state_.cameraHeight, character.eyeHeight) ||
        !nearlyEqual(state_.horizontalSpeed, characterMotor_.horizontalSpeed())) return false;
    if (state_.stance != character.stance || state_.grounded != character.grounded || state_.sprinting != character.sprinting) return false;
    return true;
}

void EngineCore::recordBlackBox(double realDeltaSeconds, std::uint32_t catchUpSteps, bool catchUpClamped) noexcept {
    BlackBoxFrame frame{};
    frame.simulationTick = state_.simulationTick;
    frame.simulationSeconds = state_.simulationSeconds;
    frame.realDeltaSeconds = realDeltaSeconds;
    frame.playerX = state_.playerX;
    frame.playerY = state_.playerY;
    frame.playerZ = state_.playerZ;
    frame.velocityX = state_.velocityX;
    frame.velocityY = state_.velocityY;
    frame.velocityZ = state_.velocityZ;
    frame.playerBodyYaw = state_.playerBodyYaw;
    frame.playerYaw = state_.playerYaw;
    frame.playerPitch = state_.playerPitch;
    frame.cameraHeight = state_.cameraHeight;
    frame.moveForward = moveForward_;
    frame.moveStrafe = moveStrafe_;
    frame.horizontalSpeed = state_.horizontalSpeed;
    frame.shotsFired = state_.shotsFired;
    frame.catchUpSteps = catchUpSteps;
    frame.stance = state_.stance;
    frame.grounded = state_.grounded;
    frame.sprinting = state_.sprinting;
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
        moveForward_ = std::numeric_limits<double>::quiet_NaN();
    });
}

void EngineCore::testOnlySetAirborne(double heightMeters, double verticalVelocity) noexcept {
    characterMotor_.testOnlySetAirborne(heightMeters, verticalVelocity);
    syncCharacterSnapshot();
}
#endif

} // namespace metse
