#pragma once
#include <cstdint>

namespace metse {

struct EngineConfig {
    double fixedStepSeconds = 1.0 / 60.0;
    std::uint32_t maxCatchUpSteps = 4;
    std::uint32_t maxCombatants = 32;
    double playerMoveSpeed = 4.5;
};

struct EngineSnapshot {
    double simulationSeconds = 0.0;
    std::uint64_t simulationTick = 0;
    std::uint32_t activeCombatants = 0;
    double interpolationAlpha = 0.0;
    double playerX = 0.0;
    double playerZ = 0.0;
    double playerYaw = 0.0;
    double playerPitch = 0.0;
    std::uint64_t shotsFired = 0;
};

class EngineCore final {
public:
    explicit EngineCore(EngineConfig config = {});
    void reset();
    void advance(double realDeltaSeconds);
    bool setActiveCombatants(std::uint32_t count);
    void setMovementInput(double forward, double strafe);
    void addLookInput(double yawDeltaRadians, double pitchDeltaRadians);
    void triggerFire();
    const EngineSnapshot& snapshot() const noexcept { return state_; }
    const EngineConfig& config() const noexcept { return config_; }

private:
    void fixedStep();
    EngineConfig config_;
    EngineSnapshot state_;
    double accumulatorSeconds_ = 0.0;
    double moveForward_ = 0.0;
    double moveStrafe_ = 0.0;
};

} // namespace metse
