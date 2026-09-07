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
    reset();
}

void EngineCore::reset() {
    state_ = {};
    accumulatorSeconds_ = 0.0;
}

bool EngineCore::setActiveCombatants(std::uint32_t count) {
    if (count > config_.maxCombatants) return false;
    state_.activeCombatants = count;
    return true;
}

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

    if (steps == config_.maxCatchUpSteps && accumulatorSeconds_ >= config_.fixedStepSeconds) {
        accumulatorSeconds_ = std::fmod(accumulatorSeconds_, config_.fixedStepSeconds);
    }

    state_.interpolationAlpha = std::clamp(accumulatorSeconds_ / config_.fixedStepSeconds, 0.0, 1.0);
}

void EngineCore::fixedStep() {
    state_.simulationSeconds += config_.fixedStepSeconds;
    ++state_.simulationTick;
}

} // namespace metse
