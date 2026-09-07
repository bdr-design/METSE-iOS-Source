#pragma once

#include "METSECharacterMotor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

struct ObservatoryFrameInput {
    std::uint64_t simulationTick = 0;
    double realDeltaSeconds = 0.0;
    double playerX = 0.0;
    double playerZ = 0.0;
    double horizontalSpeed = 0.0;
    CharacterStance stance = CharacterStance::Standing;
    CharacterGait gait = CharacterGait::Idle;
    bool grounded = true;
    bool sprinting = false;
    std::uint32_t catchUpSteps = 0;
    bool catchUpClamped = false;
    std::uint32_t collisionContacts = 0;
};

struct ObservatoryFrame {
    std::uint64_t simulationTick = 0;
    double frameMilliseconds = 0.0;
    double horizontalSpeed = 0.0;
    std::uint32_t catchUpSteps = 0;
    std::uint32_t collisionContacts = 0;
    CharacterStance stance = CharacterStance::Standing;
    CharacterGait gait = CharacterGait::Idle;
    bool grounded = true;
    bool sprinting = false;
    bool catchUpClamped = false;
};

struct ObservatoryReport {
    std::uint64_t observedFrames = 0;
    std::size_t retainedFrames = 0;
    double averageFrameMilliseconds = 0.0;
    double p95FrameMilliseconds = 0.0;
    double maxFrameMilliseconds = 0.0;
    double estimatedFPS = 0.0;
    std::uint64_t framesOver20ms = 0;
    std::uint64_t framesOver33ms = 0;
    std::uint64_t catchUpClampedFrames = 0;
    std::uint64_t totalCollisionContacts = 0;
    double distanceTravelled = 0.0;
    double peakHorizontalSpeed = 0.0;
    double sprintSeconds = 0.0;
    double airborneSeconds = 0.0;
    double standingSeconds = 0.0;
    double crouchedSeconds = 0.0;
    double proneSeconds = 0.0;
    std::uint64_t stanceTransitions = 0;
    std::uint64_t gaitTransitions = 0;
};

class ObservatoryCore final {
public:
    static constexpr std::size_t kFrameCapacity = 600;

    void reset() noexcept;
    void observe(const ObservatoryFrameInput& input) noexcept;

    [[nodiscard]] ObservatoryReport report() const noexcept;
    [[nodiscard]] bool newestFrame(std::size_t offset, ObservatoryFrame& out) const noexcept;
    [[nodiscard]] std::size_t retainedFrameCount() const noexcept { return frameCount_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    std::array<ObservatoryFrame, kFrameCapacity> frames_{};
    std::size_t frameWrite_ = 0;
    std::size_t frameCount_ = 0;
    std::uint64_t observedFrames_ = 0;
    std::uint64_t framesOver20ms_ = 0;
    std::uint64_t framesOver33ms_ = 0;
    std::uint64_t catchUpClampedFrames_ = 0;
    std::uint64_t totalCollisionContacts_ = 0;
    double distanceTravelled_ = 0.0;
    double peakHorizontalSpeed_ = 0.0;
    double sprintSeconds_ = 0.0;
    double airborneSeconds_ = 0.0;
    double standingSeconds_ = 0.0;
    double crouchedSeconds_ = 0.0;
    double proneSeconds_ = 0.0;
    std::uint64_t stanceTransitions_ = 0;
    std::uint64_t gaitTransitions_ = 0;
    bool hasPreviousPosition_ = false;
    double previousX_ = 0.0;
    double previousZ_ = 0.0;
    CharacterStance previousStance_ = CharacterStance::Standing;
    CharacterGait previousGait_ = CharacterGait::Idle;
};

} // namespace metse
