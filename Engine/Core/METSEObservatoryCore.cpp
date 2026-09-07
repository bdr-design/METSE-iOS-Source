#include "METSEObservatoryCore.hpp"

#include <algorithm>
#include <cmath>

namespace metse {

void ObservatoryCore::reset() noexcept {
    frames_ = {};
    frameWrite_ = 0;
    frameCount_ = 0;
    observedFrames_ = 0;
    framesOver20ms_ = 0;
    framesOver33ms_ = 0;
    catchUpClampedFrames_ = 0;
    totalCollisionContacts_ = 0;
    distanceTravelled_ = 0.0;
    peakHorizontalSpeed_ = 0.0;
    sprintSeconds_ = 0.0;
    airborneSeconds_ = 0.0;
    standingSeconds_ = 0.0;
    crouchedSeconds_ = 0.0;
    proneSeconds_ = 0.0;
    stanceTransitions_ = 0;
    gaitTransitions_ = 0;
    hasPreviousPosition_ = false;
    previousX_ = 0.0;
    previousZ_ = 0.0;
    previousStance_ = CharacterStance::Standing;
    previousGait_ = CharacterGait::Idle;
}

void ObservatoryCore::observe(const ObservatoryFrameInput& input) noexcept {
    if (!std::isfinite(input.realDeltaSeconds) || input.realDeltaSeconds < 0.0 ||
        !std::isfinite(input.playerX) || !std::isfinite(input.playerZ) ||
        !std::isfinite(input.horizontalSpeed) || input.horizontalSpeed < 0.0) return;

    const double frameMs = input.realDeltaSeconds * 1000.0;
    ObservatoryFrame frame{};
    frame.simulationTick = input.simulationTick;
    frame.frameMilliseconds = frameMs;
    frame.horizontalSpeed = input.horizontalSpeed;
    frame.catchUpSteps = input.catchUpSteps;
    frame.collisionContacts = input.collisionContacts;
    frame.stance = input.stance;
    frame.gait = input.gait;
    frame.grounded = input.grounded;
    frame.sprinting = input.sprinting;
    frame.catchUpClamped = input.catchUpClamped;
    frames_[frameWrite_] = frame;
    frameWrite_ = (frameWrite_ + 1) % kFrameCapacity;
    frameCount_ = std::min(frameCount_ + 1, kFrameCapacity);

    ++observedFrames_;
    if (frameMs > 20.0) ++framesOver20ms_;
    if (frameMs > 33.333333) ++framesOver33ms_;
    if (input.catchUpClamped) ++catchUpClampedFrames_;
    totalCollisionContacts_ += input.collisionContacts;
    peakHorizontalSpeed_ = std::max(peakHorizontalSpeed_, input.horizontalSpeed);

    if (hasPreviousPosition_) {
        const double distance = std::hypot(input.playerX - previousX_, input.playerZ - previousZ_);
        if (std::isfinite(distance) && distance < 5.0) distanceTravelled_ += distance;
        if (input.stance != previousStance_) ++stanceTransitions_;
        if (input.gait != previousGait_) ++gaitTransitions_;
    }
    previousX_ = input.playerX;
    previousZ_ = input.playerZ;
    previousStance_ = input.stance;
    previousGait_ = input.gait;
    hasPreviousPosition_ = true;

    if (input.sprinting) sprintSeconds_ += input.realDeltaSeconds;
    if (!input.grounded) airborneSeconds_ += input.realDeltaSeconds;
    switch (input.stance) {
        case CharacterStance::Standing: standingSeconds_ += input.realDeltaSeconds; break;
        case CharacterStance::Crouched: crouchedSeconds_ += input.realDeltaSeconds; break;
        case CharacterStance::Prone: proneSeconds_ += input.realDeltaSeconds; break;
    }
}

ObservatoryReport ObservatoryCore::report() const noexcept {
    ObservatoryReport out{};
    out.observedFrames = observedFrames_;
    out.retainedFrames = frameCount_;
    out.framesOver20ms = framesOver20ms_;
    out.framesOver33ms = framesOver33ms_;
    out.catchUpClampedFrames = catchUpClampedFrames_;
    out.totalCollisionContacts = totalCollisionContacts_;
    out.distanceTravelled = distanceTravelled_;
    out.peakHorizontalSpeed = peakHorizontalSpeed_;
    out.sprintSeconds = sprintSeconds_;
    out.airborneSeconds = airborneSeconds_;
    out.standingSeconds = standingSeconds_;
    out.crouchedSeconds = crouchedSeconds_;
    out.proneSeconds = proneSeconds_;
    out.stanceTransitions = stanceTransitions_;
    out.gaitTransitions = gaitTransitions_;
    if (frameCount_ == 0) return out;

    std::array<double, kFrameCapacity> values{};
    double sum = 0.0;
    double maxValue = 0.0;
    const std::size_t oldest = (frameWrite_ + kFrameCapacity - frameCount_) % kFrameCapacity;
    for (std::size_t i = 0; i < frameCount_; ++i) {
        const double value = frames_[(oldest + i) % kFrameCapacity].frameMilliseconds;
        values[i] = value;
        sum += value;
        maxValue = std::max(maxValue, value);
    }
    std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(frameCount_));
    const std::size_t p95Index = std::min(frameCount_ - 1,
        static_cast<std::size_t>(std::ceil(static_cast<double>(frameCount_) * 0.95)) - 1);
    out.averageFrameMilliseconds = sum / static_cast<double>(frameCount_);
    out.p95FrameMilliseconds = values[p95Index];
    out.maxFrameMilliseconds = maxValue;
    if (out.averageFrameMilliseconds > 0.000001) out.estimatedFPS = 1000.0 / out.averageFrameMilliseconds;
    return out;
}

bool ObservatoryCore::newestFrame(std::size_t offset, ObservatoryFrame& out) const noexcept {
    if (offset >= frameCount_) return false;
    const std::size_t index = (frameWrite_ + kFrameCapacity - 1 - offset) % kFrameCapacity;
    out = frames_[index];
    return true;
}

bool ObservatoryCore::validate() const noexcept {
    if (frameCount_ > kFrameCapacity || frameWrite_ >= kFrameCapacity) return false;
    if (!std::isfinite(distanceTravelled_) || distanceTravelled_ < 0.0) return false;
    if (!std::isfinite(peakHorizontalSpeed_) || peakHorizontalSpeed_ < 0.0) return false;
    if (!std::isfinite(sprintSeconds_) || sprintSeconds_ < 0.0) return false;
    if (!std::isfinite(airborneSeconds_) || airborneSeconds_ < 0.0) return false;
    if (!std::isfinite(standingSeconds_) || !std::isfinite(crouchedSeconds_) || !std::isfinite(proneSeconds_)) return false;
    for (std::size_t i = 0; i < frameCount_; ++i) {
        ObservatoryFrame frame{};
        if (!newestFrame(i, frame)) return false;
        if (!std::isfinite(frame.frameMilliseconds) || frame.frameMilliseconds < 0.0) return false;
        if (!std::isfinite(frame.horizontalSpeed) || frame.horizontalSpeed < 0.0) return false;
    }
    return true;
}

} // namespace metse
