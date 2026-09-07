#pragma once

#include "METSECharacterMotor.hpp"
#include "METSEIntegrityCore.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace metse {

struct EngineConfig {
    double fixedStepSeconds = 1.0 / 60.0;
    std::uint32_t maxCatchUpSteps = 4;
    std::uint32_t maxCombatants = 32;
    CharacterConfig character{};
};

struct EngineSnapshot {
    double simulationSeconds = 0.0;
    std::uint64_t simulationTick = 0;
    std::uint32_t activeCombatants = 0;
    double interpolationAlpha = 0.0;
    double playerX = 0.0;
    double playerY = 0.0;
    double playerZ = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    double playerBodyYaw = 0.0;
    double playerYaw = 0.0;
    double playerPitch = 0.0;
    double cameraHeight = 1.64;
    double horizontalSpeed = 0.0;
    CharacterStance stance = CharacterStance::Standing;
    bool grounded = true;
    bool sprinting = false;
    std::uint64_t shotsFired = 0;
};

struct BlackBoxFrame {
    std::uint64_t simulationTick = 0;
    double simulationSeconds = 0.0;
    double realDeltaSeconds = 0.0;
    double playerX = 0.0;
    double playerY = 0.0;
    double playerZ = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    double playerBodyYaw = 0.0;
    double playerYaw = 0.0;
    double playerPitch = 0.0;
    double cameraHeight = 0.0;
    double moveForward = 0.0;
    double moveStrafe = 0.0;
    double horizontalSpeed = 0.0;
    std::uint64_t shotsFired = 0;
    std::uint32_t catchUpSteps = 0;
    CharacterStance stance = CharacterStance::Standing;
    bool grounded = true;
    bool sprinting = false;
    bool catchUpClamped = false;
};

struct EngineDiagnostics {
    IntegrityMetrics integrity{};
    Sha256Digest journalHead{};
    std::size_t retainedEvents = 0;
    std::size_t retainedCommands = 0;
    std::size_t retainedBlackBoxFrames = 0;
    bool journalValid = false;
};

class EngineCore final {
public:
    static constexpr std::size_t kBlackBoxCapacity = 720;

    explicit EngineCore(EngineConfig config = {});

    void reset();
    void advance(double realDeltaSeconds);
    bool setActiveCombatants(std::uint32_t count);
    void setMovementInput(double forward, double strafe);
    void addLookInput(double yawDeltaRadians, double pitchDeltaRadians);
    void setSprintHeld(bool held);
    void cycleStance();
    void triggerFire();

    [[nodiscard]] const EngineSnapshot& snapshot() const noexcept { return state_; }
    [[nodiscard]] const EngineConfig& config() const noexcept { return config_; }
    [[nodiscard]] EngineDiagnostics diagnostics() const noexcept;
    [[nodiscard]] bool newestBlackBoxFrame(std::size_t offset, BlackBoxFrame& out) const noexcept;
    [[nodiscard]] bool newestEvent(std::size_t offset, EventRecord& out) const noexcept { return integrity_.newestEvent(offset, out); }
    [[nodiscard]] bool newestCommand(std::size_t offset, CommandRecord& out) const noexcept { return integrity_.newestCommand(offset, out); }

#ifdef METSE_TESTING
    bool testOnlyExecuteInvariantViolation();
    void testOnlySetAirborne(double heightMeters, double verticalVelocity) noexcept;
#endif

private:
    struct MutationCheckpoint {
        EngineSnapshot state{};
        CharacterMotor characterMotor{};
        double accumulatorSeconds = 0.0;
        double moveForward = 0.0;
        double moveStrafe = 0.0;
        bool sprintHeld = false;
    };

    template <typename Apply>
    bool executeAtomic(CommandKind commandKind,
                       bool precondition,
                       EventKind domainEvent,
                       Apply&& apply) {
        const std::uint64_t commandId = integrity_.admit(commandKind, state_.simulationTick);
        if (!precondition) {
            integrity_.reject(commandId, state_.simulationTick);
            return false;
        }

        const MutationCheckpoint checkpoint{state_, characterMotor_, accumulatorSeconds_, moveForward_, moveStrafe_, sprintHeld_};
        apply();
        syncCharacterSnapshot();
        if (!validateInvariants()) {
            state_ = checkpoint.state;
            characterMotor_ = checkpoint.characterMotor;
            accumulatorSeconds_ = checkpoint.accumulatorSeconds;
            moveForward_ = checkpoint.moveForward;
            moveStrafe_ = checkpoint.moveStrafe;
            sprintHeld_ = checkpoint.sprintHeld;
            integrity_.rollback(commandId, state_.simulationTick);
            return false;
        }

        integrity_.commit(commandId, state_.simulationTick);
        integrity_.appendDomainEvent(domainEvent, commandId, state_.simulationTick);
        return true;
    }

    void resetState() noexcept;
    void fixedStep() noexcept;
    void syncCharacterSnapshot() noexcept;
    bool validateInvariants() const noexcept;
    void recordBlackBox(double realDeltaSeconds,
                        std::uint32_t catchUpSteps,
                        bool catchUpClamped) noexcept;

    EngineConfig config_{};
    EngineSnapshot state_{};
    CharacterMotor characterMotor_{};
    double accumulatorSeconds_ = 0.0;
    double moveForward_ = 0.0;
    double moveStrafe_ = 0.0;
    bool sprintHeld_ = false;
    IntegrityCore integrity_{};
    std::array<BlackBoxFrame, kBlackBoxCapacity> blackBox_{};
    std::size_t blackBoxWrite_ = 0;
    std::size_t blackBoxCount_ = 0;
};

} // namespace metse
